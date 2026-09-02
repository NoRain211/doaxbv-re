#include "d3d_draw_adapter.h"
#include "d3d_presenter.h"
#include "d3d_render_state_adapter.h"
#include "d3d_frame_adapter.h"
#include "d3d_texture_adapter.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    D3D_DEVICE_DRAW_INDEXED_VERTICES_ADDRESS = 0x001e78b0u,
    D3D_DEVICE_GLOBAL = 0x001f2978u,
    /* D3D_CommonSetRenderTarget (0x001e5c40) stores the current render
       target at device dword 0x86d and the back buffer at dword 0x870. */
    D3D_CURRENT_RENDER_TARGET_OFFSET = 0x86du * 4u,
    D3D_BACK_BUFFER_OFFSET = 0x870u * 4u,
    /* Stream 0 binding written by D3DDevice_SetStreamSource. Each stream
       occupies 12 bytes: stride at 0x1f2e20, the vertex buffer object at
       0x1f2e28. */
    D3D_STREAM0_STRIDE = 0x001f2e20u,
    D3D_STREAM0_BUFFER = 0x001f2e28u,
    /* X_D3DResource: Common at +0, Data at +4. */
    D3D_RESOURCE_DATA_OFFSET = 0x04u,
    D3D_RESOURCE_TYPE_MASK = 0x00070000u,
    D3D_RESOURCE_TYPE_VERTEXBUFFER = 0x00000000u,
    /* Fixed-function vertex shader handle, set by D3DDevice_SetVertexShader.
       Bit 0 clear means the handle is an FVF rather than a program. */
    D3D_VERTEX_SHADER_HANDLE_OFFSET = 0x0384u,
    /* D3DDevice_SetTransform copies 16 floats to device + 0x810 + index * 0x40. */
    D3D_TRANSFORM_BASE_OFFSET = 0x0810u,
    D3D_TRANSFORM_STRIDE = 0x0040u,
    D3D_TRANSFORM_VIEW = 0u,
    D3D_TRANSFORM_PROJECTION = 1u,
    D3D_TRANSFORM_WORLD = 6u,
};

void sub_001E78B0(void);

static RecompD3dDrawState draw_state;
static uint32_t draw_submitted;
static uint32_t draw_declined;
static uint32_t draw_unbound;
static uint32_t draw_unsupported_formats[256];
static uint32_t draw_unmapped_formats[256];

/* Every distinct FVF that reached the presenter, with how many draws used it.
   The presenter's input layout names fixed component offsets, so a second FVF
   with a different layout would be read at the wrong offsets; this makes that
   silent case visible in the run tally. */
enum { DRAW_FVF_SLOTS = 8u };
static uint32_t draw_fvf_seen[DRAW_FVF_SLOTS];
static uint32_t draw_fvf_count[DRAW_FVF_SLOTS];
static uint32_t draw_fvf_used;
static uint32_t draw_fvf_overflow;

static void record_fvf(uint32_t fvf)
{
    for (uint32_t i = 0u; i < draw_fvf_used; ++i) {
        if (draw_fvf_seen[i] == fvf) {
            ++draw_fvf_count[i];
            return;
        }
    }
    if (draw_fvf_used == DRAW_FVF_SLOTS) {
        ++draw_fvf_overflow;
        return;
    }
    draw_fvf_seen[draw_fvf_used] = fvf;
    draw_fvf_count[draw_fvf_used] = 1u;
    ++draw_fvf_used;
}

void recomp_d3d_draw_adapter_report_fvf(void)
{
    for (uint32_t i = 0u; i < draw_fvf_used; ++i) {
        fprintf(
            stderr,
            "recomp d3d draw: fvf 0x%08" PRIx32 " stride=%" PRIu32
            " draws=%" PRIu32 "\n",
            draw_fvf_seen[i],
            recomp_d3d_fvf_stride(draw_fvf_seen[i]),
            draw_fvf_count[i]);
    }
    if (draw_fvf_overflow != 0u) {
        fprintf(
            stderr,
            "recomp d3d draw: fvf table overflow draws=%" PRIu32 "\n",
            draw_fvf_overflow);
    }
    fprintf(
        stderr,
        "recomp d3d draw: stage0 unbound draws=%" PRIu32 "\n",
        draw_unbound);
    for (uint32_t format = 0u; format < 256u; ++format) {
        if (draw_unsupported_formats[format] != 0u ||
            draw_unmapped_formats[format] != 0u) {
            fprintf(
                stderr,
                "recomp d3d draw: stage0 fmt=0x%02" PRIx32
                " unsupported=%" PRIu32 " unmapped=%" PRIu32 "\n",
                format,
                draw_unsupported_formats[format],
                draw_unmapped_formats[format]);
        }
    }
}

void recomp_d3d_draw_adapter_reset(void)
{
    recomp_d3d_draw_reset(&draw_state);
    draw_submitted = 0u;
    draw_declined = 0u;
    memset(draw_fvf_seen, 0, sizeof draw_fvf_seen);
    memset(draw_fvf_count, 0, sizeof draw_fvf_count);
    draw_fvf_used = 0u;
    draw_fvf_overflow = 0u;
}

uint32_t recomp_d3d_draw_adapter_submitted(void)
{
    return draw_submitted;
}

uint32_t recomp_d3d_draw_adapter_declined(void)
{
    return draw_declined;
}

static uint32_t stack_argument(uint32_t entry_esp, uint32_t index)
{
    return *recomp_memory_u32(entry_esp + 4u + index * 4u);
}

/* Resolves a guest span to a host pointer without the fail-loud path in
   recomp_memory(): a draw that names an unmapped buffer must be declined,
   not turned into a runtime stop. Mirrors the runtime's cached-alias
   fallback so kseg0 pointers resolve the same way. */
static const uint8_t *guest_span(uint32_t address, uint32_t size)
{
    uint64_t span_end = (uint64_t)address + size;

    if (size == 0u || span_end > 0x100000000u) {
        return NULL;
    }
    for (int alias = 0; alias < 2; ++alias) {
        uint32_t base = address;

        if (alias == 1) {
            if (address < 0x80000000u) {
                break;
            }
            base = address - 0x80000000u;
        }
        for (size_t i = 0u; i < recomp_runtime.memory_region_count; ++i) {
            const RecompMemoryRegion *region =
                &recomp_runtime.memory_regions[i];
            uint64_t region_end = (uint64_t)region->address + region->size;

            if (region->data != NULL && base >= region->address &&
                (uint64_t)base + size <= region_end) {
                return region->data + (base - region->address);
            }
        }
    }
    return NULL;
}

static bool read_transform(
    uint32_t device,
    uint32_t index,
    float destination[16])
{
    uint32_t address =
        device + D3D_TRANSFORM_BASE_OFFSET + index * D3D_TRANSFORM_STRIDE;
    const uint8_t *source = guest_span(address, 64u);

    if (source == NULL) {
        return false;
    }
    memcpy(destination, source, 64u);
    return true;
}

/* Row-vector multiply: D3D transforms a vertex as v * M, so the composite is
   world * view * projection in that order. */
static void multiply_transform(
    const float left[16],
    const float right[16],
    float destination[16])
{
    for (uint32_t row = 0u; row < 4u; ++row) {
        for (uint32_t column = 0u; column < 4u; ++column) {
            float sum = 0.0f;

            for (uint32_t k = 0u; k < 4u; ++k) {
                sum += left[row * 4u + k] * right[k * 4u + column];
            }
            destination[row * 4u + column] = sum;
        }
    }
}

static bool transform_is_usable(const float matrix[16])
{
    /* An all-zero matrix collapses every vertex to the origin; treat it as
       "not set yet" rather than drawing a degenerate point. */
    for (uint32_t i = 0u; i < 16u; ++i) {
        if (matrix[i] != 0.0f) {
            return true;
        }
    }
    return false;
}

static bool compose_world_view_projection(uint32_t device, float result[16])
{
    float world[16];
    float view[16];
    float projection[16];
    float world_view[16];

    if (!read_transform(device, D3D_TRANSFORM_WORLD, world) ||
        !read_transform(device, D3D_TRANSFORM_VIEW, view) ||
        !read_transform(device, D3D_TRANSFORM_PROJECTION, projection)) {
        return false;
    }
    {
        /* Measured 2026-08-26: these slots read back sparse - only floats 0, 8
           and 12 are non-zero for VIEW and WORLD - so the composite collapses
           and every vertex clips. The offset (device + 0x810 + index * 0x40) is
           confirmed correct against D3DDevice_SetTransform at 0x001E36D0, so
           the defect is in what reaches that memory, not in where it is read.
           Reported once so the next session starts from the measurement. */
        static bool reported;

        if (!reported) {
            reported = true;
            for (uint32_t slot = 0u; slot < 7u; ++slot) {
                float m[16];

                if (!read_transform(device, slot, m)) {
                    continue;
                }
                fprintf(
                    stderr,
                    "recomp d3d draw: xform[%u] %g %g %g %g | %g %g %g %g | "
                    "%g %g %g %g | %g %g %g %g\n",
                    slot, m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
                    m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
            }
        }
    }
    if (!transform_is_usable(world) || !transform_is_usable(view) ||
        !transform_is_usable(projection)) {
        return false;
    }
    /* The composite's Y row alternates by 2x every frame, which shows up as
       the image jumping vertically. Report the three source slots so the
       alternation can be attributed to one of them rather than to the
       multiply. Opt-in, bounded, and off by default. */
    {
        static const char *slot_trace;
        static bool slot_trace_read;
        static uint32_t slot_trace_lines;

        if (!slot_trace_read) {
            slot_trace_read = true;
            slot_trace = getenv("RECOMP_D3D_SLOTTRACE");
        }
        if (slot_trace != NULL && slot_trace_lines < 240u) {
            static uint32_t slot_trace_last_swap = 0xffffffffu;
            const uint32_t swap = recomp_d3d_frame_adapter_swap_counter();

            /* One line per draw fills the budget inside a single frame, and
               the question is how a frame differs from the next one. Sample
               the first composed draw of each frame instead. */
            if (swap == slot_trace_last_swap) {
                goto slot_trace_done;
            }
            slot_trace_last_swap = swap;
            ++slot_trace_lines;
            fprintf(
                stderr,
                "recomp d3d slottrace: swap=%" PRIu32 " world5=%g world13=%g "
                "view5=%g view13=%g proj5=%g proj13=%g\n",
                swap, world[5], world[13], view[5], view[13],
                projection[5], projection[13]);
            fprintf(
                stderr,
                "recomp d3d viewfull: swap=%" PRIu32
                " %g %g %g %g | %g %g %g %g | %g %g %g %g | %g %g %g %g\n",
                swap,
                view[0], view[1], view[2], view[3],
                view[4], view[5], view[6], view[7],
                view[8], view[9], view[10], view[11],
                view[12], view[13], view[14], view[15]);
        }
slot_trace_done:
        ;
    }
    multiply_transform(world, view, world_view);
    multiply_transform(world_view, projection, result);
    return true;
}

static uint32_t largest_index(const uint8_t *indices, uint32_t count)
{
    uint32_t largest = 0u;

    for (uint32_t i = 0u; i < count; ++i) {
        uint16_t value;

        memcpy(&value, indices + i * 2u, sizeof value);
        if (value > largest) {
            largest = value;
        }
    }
    return largest;
}

static void report_decline(const char *reason)
{
    static uint32_t reported;

    ++draw_declined;
    /* One line per distinct reason bucket keeps a declining seam visible
       without flooding a 120 Hz frame loop. */
    if (reported < 8u) {
        ++reported;
        fprintf(stderr, "recomp d3d draw: declined (%s)\n", reason);
    }
}

/* Byte size of one swizzled surface the presenter knows how to upload. The
   block-compressed formats are stored linearly and upload as-is; the
   uncompressed ones are Morton-ordered and the presenter unswizzles them. */
static bool swizzled_byte_count(
    const RecompD3dTextureDesc *desc,
    uint32_t *out)
{
    uint32_t blocks_wide;
    uint32_t blocks_high;

    if (desc->linear || desc->width == 0u || desc->height == 0u) {
        return false;
    }
    if (desc->format_byte == RECOMP_D3D_TEXTURE_FORMAT_A8R8G8B8 ||
        desc->format_byte == RECOMP_D3D_TEXTURE_FORMAT_A8) {
        *out = desc->width * desc->height * (desc->bits_per_pixel / 8u);
        return true;
    }
    if (desc->format_byte != RECOMP_D3D_TEXTURE_FORMAT_DXT1 &&
        desc->format_byte != RECOMP_D3D_TEXTURE_FORMAT_DXT3 &&
        desc->format_byte != RECOMP_D3D_TEXTURE_FORMAT_DXT5) {
        return false;
    }
    blocks_wide = (desc->width + 3u) / 4u;
    blocks_high = (desc->height + 3u) / 4u;
    *out = blocks_wide * blocks_high *
        (desc->format_byte == RECOMP_D3D_TEXTURE_FORMAT_DXT1 ? 8u : 16u);
    return true;
}

static void attach_stage0_texture(RecompD3dPresenterDrawCommand *draw)
{
    const RecompD3dTextureDesc *desc = recomp_d3d_texture_adapter_stage(0u);
    const uint8_t *bytes;
    uint32_t byte_count;

    if (desc == NULL || desc->data == 0u) {
        ++draw_unbound;
        return;
    }
    if (!swizzled_byte_count(desc, &byte_count)) {
        /* An unsupported format draws untextured, which otherwise looks
           identical to a draw the guest never bound a texture for. */
        ++draw_unsupported_formats[desc->format_byte & 0xffu];
        return;
    }
    bytes = guest_span(desc->data, byte_count);
    if (bytes == NULL) {
        ++draw_unmapped_formats[desc->format_byte & 0xffu];
        return;
    }
    draw->texture = *desc;
    draw->has_texture = true;
    draw->texture_bytes = bytes;
    draw->texture_byte_count = byte_count;
}

static void recomp_d3d_draw_indexed_vertices_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t primitive_type = stack_argument(entry_esp, 0u);
    uint32_t index_count = stack_argument(entry_esp, 1u);
    uint32_t index_data = stack_argument(entry_esp, 2u);
    uint32_t device = *recomp_memory_u32(D3D_DEVICE_GLOBAL);
    uint32_t vertex_buffer;
    uint32_t vertex_data = 0u;
    RecompD3dDrawResult result;
    RecompD3dPresenterCommand command;
    const uint8_t *index_bytes;
    const uint8_t *vertex_bytes;
    uint32_t vertex_span;
    uint32_t vertex_count;
    float transform[16];

    /* Run the guest's own driver body first so all push-buffer and fence
       bookkeeping stays byte-identical to the uninterceped run; the host
       draw is added alongside it, never in place of it. This also leaves
       ESP adjusted exactly as the generated body leaves it. */
    sub_001E78B0();

    draw_state.stream0.stride = *recomp_memory_u32(D3D_STREAM0_STRIDE);
    vertex_buffer = *recomp_memory_u32(D3D_STREAM0_BUFFER);
    if (vertex_buffer != 0u &&
        guest_span(vertex_buffer, 8u) != NULL &&
        (*recomp_memory_u32(vertex_buffer) & D3D_RESOURCE_TYPE_MASK) ==
            D3D_RESOURCE_TYPE_VERTEXBUFFER) {
        vertex_data =
            *recomp_memory_u32(vertex_buffer + D3D_RESOURCE_DATA_OFFSET);
    }
    draw_state.stream0.vertex_data = vertex_data;
    if (device != 0u) {
        uint32_t handle =
            *recomp_memory_u32(device + D3D_VERTEX_SHADER_HANDLE_OFFSET);

        /* Bit 0 set means a vertex program, whose output layout this seam
           cannot infer. Leave the FVF at zero so the plan is declined. */
        draw_state.fvf = (handle & 1u) == 0u ? handle : 0u;
    }

    result = recomp_d3d_draw_indexed(
        &draw_state, primitive_type, index_count, index_data);
    if (result.error != RECOMP_D3D_DRAW_OK) {
        report_decline("plan");
        return;
    }
    if (recomp_d3d_fvf_stride(result.plan.fvf) != result.plan.vertex_stride) {
        report_decline("fvf");
        return;
    }

    index_bytes = guest_span(result.plan.index_data, result.plan.index_bytes);
    if (index_bytes == NULL) {
        report_decline("indices");
        return;
    }
    vertex_count = largest_index(index_bytes, result.plan.index_count) + 1u;
    vertex_span = recomp_d3d_draw_vertex_bytes(
        result.plan.vertex_stride, vertex_count - 1u);
    vertex_bytes = guest_span(result.plan.vertex_data, vertex_span);
    if (vertex_bytes == NULL) {
        report_decline("vertices");
        return;
    }
    if (device == 0u || !compose_world_view_projection(device, transform)) {
        report_decline("transform");
        return;
    }

    memset(&command, 0, sizeof command);
    command.type = RECOMP_D3D_PRESENTER_COMMAND_DRAW;
    /* Round 21 probe. FUN_001221e0 and FUN_001225f0 are render-to-texture
       passes: they call GetRenderTarget2, bind a texture surface with
       SetRenderTarget, draw, then restore. The runtime intercepts no
       render-target entry point, so those off-screen draws are submitted
       to the host as if they were scene geometry. An earlier probe sampled
       only the first composed draw of each frame, which cannot observe a
       pass that opens and closes inside one frame. Count per draw instead,
       and report the split once per frame. Opt-in, off by default. */
    {
        static const char *rt_trace;
        static bool rt_trace_read;
        static uint32_t rt_trace_lines;
        static uint32_t rt_trace_last_swap = 0xffffffffu;
        static uint32_t rt_on_backbuffer;
        static uint32_t rt_off_backbuffer;
        static uint32_t rt_view_changes;
        static float rt_view_first;
        static float rt_view_other;

        if (!rt_trace_read) {
            rt_trace_read = true;
            rt_trace = getenv("RECOMP_D3D_RTTRACE");
        }
        if (rt_trace != NULL) {
            const uint32_t swap = recomp_d3d_frame_adapter_swap_counter();
            const uint32_t target = *recomp_memory_u32(
                device + D3D_CURRENT_RENDER_TARGET_OFFSET);
            const uint32_t back = *recomp_memory_u32(
                device + D3D_BACK_BUFFER_OFFSET);
            /* The view slot is what alternates. Sampling it once per frame
               cannot tell "one camera that changes between frames" from
               "two cameras used within one frame". Count distinct values of
               view[13] inside each frame to separate those. */
            float view_now[16];
            float y = 0.0f;

            if (read_transform(device, D3D_TRANSFORM_VIEW, view_now)) {
                y = view_now[13];
            }
            if (swap == rt_trace_last_swap) {
                if (y != rt_view_first) {
                    ++rt_view_changes;
                    rt_view_other = y;
                }
            }

            if (swap != rt_trace_last_swap) {
                if (rt_trace_last_swap != 0xffffffffu &&
                    rt_trace_lines < 240u) {
                    ++rt_trace_lines;
                    fprintf(
                        stderr,
                        "recomp d3d rttrace: swap=%" PRIu32
                        " onbb=%" PRIu32 " offbb=%" PRIu32
                        " viewchg=%" PRIu32 " y0=%g y1=%g\n",
                        rt_trace_last_swap, rt_on_backbuffer,
                        rt_off_backbuffer, rt_view_changes,
                        rt_view_first, rt_view_other);
                }
                rt_trace_last_swap = swap;
                rt_on_backbuffer = 0u;
                rt_off_backbuffer = 0u;
                rt_view_changes = 0u;
                rt_view_first = y;
                rt_view_other = y;
            }
            if (target == back) {
                ++rt_on_backbuffer;
            } else {
                ++rt_off_backbuffer;
            }
        }
    }
    /* Round 22 probe. Round 21 proved the roll is two scenes presented
       alternately rather than one camera moving, so the open question is
       which guest value selects the scene. The frame entry sub_0011F250
       takes a view index as a stack argument, and the neighbouring camera
       copy picks its source block with MEM8(0x4D56E4) and its viewport with
       a 0..3 loop counter. A second table at 0x2961F0 is indexed by the
       scene id MEM8(slot + 0x4D92F0). Sample all three once per frame and
       report whether any of them alternates with frame parity. Opt-in, off
       by default. */
    {
        static const char *view_trace;
        static bool view_trace_read;
        static uint32_t view_trace_lines;
        static uint32_t view_trace_last_swap = 0xffffffffu;

        if (!view_trace_read) {
            view_trace_read = true;
            view_trace = getenv("RECOMP_D3D_VIEWSEL");
        }
        if (view_trace != NULL) {
            const uint32_t swap = recomp_d3d_frame_adapter_swap_counter();

            if (swap != view_trace_last_swap) {
                view_trace_last_swap = swap;
                if (view_trace_lines < 240u) {
                    const uint32_t slot =
                        (uint32_t)(*recomp_memory_i8(0x004D56E4u)) & 0xffu;
                    const uint32_t scene =
                        (uint32_t)(*recomp_memory_i8(0x004D92F0u + slot)) &
                        0xffu;
                    const uint32_t camera_base =
                        0x009EEE70u + slot * 0x1B0u;
                    float view_now[16];
                    float y = 0.0f;

                    if (read_transform(device, D3D_TRANSFORM_VIEW, view_now)) {
                        y = view_now[13];
                    }
                    ++view_trace_lines;
                    fprintf(
                        stderr,
                        "recomp d3d viewsel: swap=%" PRIu32
                        " slot=%" PRIu32 " scene=%" PRIu32
                        " cam=%08" PRIx32 " camy=%g y=%g\n",
                        swap, slot, scene, camera_base,
                        *(const float *)(const void *)recomp_memory_u32(
                            camera_base + 0x34u),
                        y);
                    /* Round 23. The selector is constant and the roll
                       survives a scene change, so the alternation is the
                       same code path with different data. Read the guest
                       camera directly: the active transform block at
                       0x009D5240 and the per-view source at 0x009EEE70,
                       whose +8/+0xC/+0x10 floats sub_0017CE00 consumes as a
                       position triple. If guest memory alternates, the game
                       computes two cameras. If it is smooth while the
                       device-tracked VIEW alternates, the defect is ours. */
                    fprintf(
                        stderr,
                        "recomp d3d camsrc: swap=%" PRIu32
                        " act=%g,%g,%g src=%g,%g,%g\n",
                        swap,
                        *(const float *)(const void *)recomp_memory_u32(
                            0x009D5240u + 0x08u),
                        *(const float *)(const void *)recomp_memory_u32(
                            0x009D5240u + 0x0Cu),
                        *(const float *)(const void *)recomp_memory_u32(
                            0x009D5240u + 0x10u),
                        *(const float *)(const void *)recomp_memory_u32(
                            0x009EEE70u + 0x08u),
                        *(const float *)(const void *)recomp_memory_u32(
                            0x009EEE70u + 0x0Cu),
                        *(const float *)(const void *)recomp_memory_u32(
                            0x009EEE70u + 0x10u));
                    /* Round 24. The render fiber sub_000C1680 gates its
                       whole render block, including the sub_001435A0 scene
                       update at 0x000C1823, on MEM8(0x9D9A45) != 0xFF, and
                       branches on 0x9D9A44/0x9D9A46/0x9D9A47 just above it.
                       Those bytes are the fiber's own view/character state.
                       If they differ on alternating swaps, the game is
                       running two different render paths per pair of frames,
                       which is the one shape consistent with a constant slot,
                       a constant scene id, and two smooth cameras. */
                    fprintf(
                        stderr,
                        "recomp d3d fiber: swap=%" PRIu32
                        " a44=%u a45=%u a46=%u a47=%u a4b=%u b=%u\n",
                        swap,
                        (unsigned)(*recomp_memory_i8(0x009D9A44u) & 0xff),
                        (unsigned)(*recomp_memory_i8(0x009D9A45u) & 0xff),
                        (unsigned)(*recomp_memory_i8(0x009D9A46u) & 0xff),
                        (unsigned)(*recomp_memory_i8(0x009D9A47u) & 0xff),
                        (unsigned)(*recomp_memory_i8(0x009D9A4Bu) & 0xff),
                        (unsigned)(*recomp_memory_i8(0x009D5884u) & 0xff));
                    /* Round 25. The scene machine sub_000A1390 is driven by
                       sub_000A0910 as a two-iteration loop, writing scene
                       ids into DAT_004d92f0[0] and [1] from the per-channel
                       request array DAT_005deac0[ch*4], with DAT_005deabe
                       [ch*4] as the channel state byte. Two live channels,
                       each with its own scene and camera, would explain two
                       smooth alternating streams while the *current* slot
                       byte stays 0. Read both channels. */
                    fprintf(
                        stderr,
                        "recomp d3d chan: swap=%" PRIu32
                        " sc0=%u sc1=%u st0=%u st1=%u rq0=%u rq1=%u\n",
                        swap,
                        (unsigned)(*recomp_memory_i8(0x004D92F0u) & 0xff),
                        (unsigned)(*recomp_memory_i8(0x004D92F1u) & 0xff),
                        (unsigned)(*recomp_memory_i8(0x005DEABEu) & 0xff),
                        (unsigned)(*recomp_memory_i8(0x005DEABEu + 4u) & 0xff),
                        (unsigned)(*recomp_memory_i8(0x005DEAC0u) & 0xff),
                        (unsigned)(*recomp_memory_i8(0x005DEAC0u + 4u) & 0xff));
                    /* Round 26. Every guest selector sampled so far is
                       constant while two independent cameras alternate, so
                       stop guessing at selectors and ask who is running.
                       The fiber adapter publishes the current fiber handle
                       into guest TLS at tls_block + 4, and the round 25 log
                       showed several fibers sharing entry 0x000b5570 with
                       similar resume counts. If alternating swaps are
                       presented from different fibers, each carrying its own
                       camera state, that explains two smooth streams with
                       no selector ever changing. */
                    {
                        const uint32_t tls_index =
                            *recomp_memory_u32(0x003B5258u);
                        const uint32_t tls_slots = *recomp_memory_u32(4u);
                        const uint32_t tls_block =
                            *recomp_memory_u32(tls_slots + tls_index * 4u);

                        fprintf(
                            stderr,
                            "recomp d3d who: swap=%" PRIu32
                            " fiber=%08" PRIx32 " tls=%08" PRIx32 "\n",
                            swap,
                            *recomp_memory_u32(tls_block + 4u),
                            tls_block);
                    }
                    /* Round 33. Rounds 22-26 hooked a function that never
                       runs and round 31 hooked one that generated C calls
                       directly, bypassing manual lookup. Both failed for the
                       same reason: they guessed at the code. Round 32 showed
                       the two VIEW streams are one camera path walked at two
                       rates, so the useful question is where that camera is
                       stored, not who writes it.

                       The device's VIEW matrix was copied from somewhere.
                       Search declared guest RAM for its bytes and report the
                       addresses. Read-only, once per frame, capped. */
                    {
                        float view_live[16];

                        if (read_transform(
                                device, D3D_TRANSFORM_VIEW, view_live)) {
                            const uint32_t device_view_address =
                                device + D3D_TRANSFORM_BASE_OFFSET +
                                D3D_TRANSFORM_VIEW * D3D_TRANSFORM_STRIDE;
                            /* Row 0 and the translation row together are 32
                               bytes and effectively unique; matching on the
                               translation alone would hit unrelated vectors. */
                            uint32_t found = 0u;
                            size_t region_index;

                            for (region_index = 0u;
                                 region_index <
                                     recomp_runtime.memory_region_count &&
                                 found < 8u;
                                 ++region_index) {
                                const RecompMemoryRegion *region =
                                    &recomp_runtime.memory_regions[
                                        region_index];
                                size_t offset;

                                if (region->data == NULL || region->size < 64u) {
                                    continue;
                                }
                                /* Guest structures are 4-byte aligned, so a
                                   dword stride is both correct and 4x cheaper
                                   than a byte-wise scan. */
                                for (offset = 0u;
                                     offset + 64u <= region->size && found < 8u;
                                     offset += 4u) {
                                    const uint32_t address =
                                        region->address + (uint32_t)offset;

                                    if (address == device_view_address) {
                                        continue;
                                    }
                                    if (memcmp(
                                            region->data + offset,
                                            view_live,
                                            16u) != 0) {
                                        continue;
                                    }
                                    if (memcmp(
                                            region->data + offset + 48u,
                                            &view_live[12],
                                            16u) != 0) {
                                        continue;
                                    }
                                    ++found;
                                    fprintf(
                                        stderr,
                                        "recomp d3d camsearch: swap=%" PRIu32
                                        " at=%08" PRIx32 " y=%g\n",
                                        swap, address, view_live[13]);
                                }
                            }
                            if (found == 0u) {
                                fprintf(
                                    stderr,
                                    "recomp d3d camsearch: swap=%" PRIu32
                                    " at=none y=%g\n",
                                    swap, view_live[13]);
                            }
                        }
                    }
                    /* Round 34. Round 33 traced the camera to a single origin
                       buffer at 0x0041A850, copied into the engine global
                       0x004D6F20 by FUN_001221E0 and handed to SetTransform.
                       One buffer, both parities. What remains is whether the
                       origin itself alternates - in which case the defect is
                       upstream in the camera update - or whether it is smooth
                       and something between it and the device introduces the
                       alternation. Sample the origin, the global, and the
                       struct header that appears to own the origin. */
                    {
                        enum {
                            CAMERA_ORIGIN = 0x0041A850u,
                            CAMERA_STRUCT = 0x0041A800u,
                            CAMERA_GLOBAL = 0x004D6F20u,
                        };
                        const uint8_t *origin =
                            guest_span(CAMERA_ORIGIN, 64u);
                        const uint8_t *global =
                            guest_span(CAMERA_GLOBAL, 64u);
                        const uint8_t *header =
                            guest_span(CAMERA_STRUCT, 16u);

                        if (origin != NULL && global != NULL) {
                            float origin_view[16];
                            float global_view[16];

                            memcpy(origin_view, origin, 64u);
                            memcpy(global_view, global, 64u);
                            fprintf(
                                stderr,
                                "recomp d3d camdelta: swap=%" PRIu32
                                " o13=%g g13=%g match=%d"
                                " o=%g,%g,%g\n",
                                swap,
                                origin_view[13],
                                global_view[13],
                                memcmp(origin_view, global_view, 64u) == 0
                                    ? 1
                                    : 0,
                                origin_view[12],
                                origin_view[13],
                                origin_view[14]);
                            if (header != NULL) {
                                uint32_t head[4];

                                memcpy(head, header, 16u);
                                fprintf(
                                    stderr,
                                    "recomp d3d camhead: swap=%" PRIu32
                                    " h0=%08" PRIx32 " h1=%08" PRIx32
                                    " h2=%08" PRIx32 " h3=%08" PRIx32 "\n",
                                    swap, head[0], head[1], head[2], head[3]);
                            }
                        }
                    }
                    /* Round 35. 0x0041A800 is not a lone struct: generated
                       code forms it as slot[index] of a five-element array
                       with stride 0xAE0, guarded by an explicit index < 5
                       bounds check. The camera matrix round 33 found is
                       slot[0] + 0x50. Round 33's search only matched the
                       CURRENT frame's matrix, so a slot holding a different
                       camera would never have shown up. Read all five. */
                    {
                        enum {
                            CAMERA_SLOT_BASE = 0x0041A800u,
                            CAMERA_SLOT_STRIDE = 0x0AE0u,
                            CAMERA_SLOT_COUNT = 5u,
                            CAMERA_SLOT_MATRIX = 0x50u,
                            CAMERA_SLOT_HEIGHT = 0x04u,
                        };
                        uint32_t slot_index;

                        for (slot_index = 0u;
                             slot_index < CAMERA_SLOT_COUNT;
                             ++slot_index) {
                            const uint32_t slot_base =
                                CAMERA_SLOT_BASE +
                                slot_index * CAMERA_SLOT_STRIDE;
                            const uint8_t *slot_matrix =
                                guest_span(
                                    slot_base + CAMERA_SLOT_MATRIX, 64u);
                            const uint8_t *slot_height =
                                guest_span(
                                    slot_base + CAMERA_SLOT_HEIGHT, 4u);

                            if (slot_matrix != NULL && slot_height != NULL) {
                                float slot_view[16];
                                float height = 0.0f;

                                memcpy(slot_view, slot_matrix, 64u);
                                memcpy(&height, slot_height, 4u);
                                fprintf(
                                    stderr,
                                    "recomp d3d camslots: swap=%" PRIu32
                                    " n=%" PRIu32 " y=%g h=%g"
                                    " t=%g,%g,%g\n",
                                    swap, slot_index,
                                    slot_view[13], height,
                                    slot_view[12], slot_view[13],
                                    slot_view[14]);
                            }
                        }
                    }
                }
            }
        }
    }
    command.data.draw.primitive_type = result.plan.primitive_type;
    command.data.draw.index_count = result.plan.index_count;
    command.data.draw.triangle_count = result.plan.triangle_count;
    command.data.draw.vertex_count = vertex_count;
    command.data.draw.vertex_stride = result.plan.vertex_stride;
    command.data.draw.fvf = result.plan.fvf;
    command.data.draw.vertex_bytes = vertex_bytes;
    command.data.draw.index_bytes = index_bytes;
    command.data.draw.has_transform = true;
    memcpy(command.data.draw.transform, transform, sizeof transform);
    recomp_d3d_depth_state(
        recomp_d3d_render_state_adapter_model(), &command.data.draw.depth);
    recomp_d3d_blend_state(
        recomp_d3d_render_state_adapter_model(), &command.data.draw.blend);
    attach_stage0_texture(&command.data.draw);

    if (recomp_d3d_presenter_submit(
            recomp_d3d_frame_adapter_presenter(), &command) !=
        RECOMP_D3D_PRESENTER_OK) {
        report_decline("presenter");
        return;
    }
    ++draw_submitted;
    record_fvf(result.plan.fvf);
}

RecompFunction recomp_d3d_draw_lookup_manual(uint32_t guest_address)
{
    return guest_address == D3D_DEVICE_DRAW_INDEXED_VERTICES_ADDRESS
        ? recomp_d3d_draw_indexed_vertices_adapter
        : NULL;
}
