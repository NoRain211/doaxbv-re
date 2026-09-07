#include "d3d_draw_adapter.h"
#include "d3d_presenter.h"
#include "d3d_render_state_adapter.h"
#include "d3d_frame_adapter.h"
#include "d3d_texture_adapter.h"

#include <inttypes.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    D3D_DEVICE_DRAW_VERTICES_UP_ADDRESS = 0x001e7750u,
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
    D3D_VERTEX_BLEND_SHADOW = 0x001f2dacu,
    /* D3DDevice_SetTransform copies 16 floats to device + 0x810 + index * 0x40. */
    D3D_TRANSFORM_BASE_OFFSET = 0x0810u,
    D3D_TRANSFORM_STRIDE = 0x0040u,
    D3D_TRANSFORM_VIEW = 0u,
    D3D_TRANSFORM_PROJECTION = 1u,
    D3D_TRANSFORM_WORLD = 6u,
};

void sub_001E7750(void);
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

/* Xbox blend modes 1/3/5 consume 1/2/3 explicit weights and the
   remaining weight. The game's matrix table selects WORLD0..WORLD3. */
static bool compose_blend_transforms(uint32_t device, RecompD3dPresenterDrawCommand *draw)
{
    RecompD3dVertexLayout layout;
    float view[16], projection[16], view_projection[16];

    if (!recomp_d3d_fvf_layout(draw->fvf, &layout)) return false;
    draw->blend_weight_count = 0u;
    if (layout.blend_weight_count == 0u ||
        *recomp_memory_u32(D3D_VERTEX_BLEND_SHADOW) == 0u) return true;
    draw->blend_weight_count = layout.blend_weight_count;
    if (*recomp_memory_u32(D3D_VERTEX_BLEND_SHADOW) != 2u * layout.blend_weight_count - 1u ||
        !read_transform(device, D3D_TRANSFORM_VIEW, view) ||
        !read_transform(device, D3D_TRANSFORM_PROJECTION, projection)) return false;
    multiply_transform(view, projection, view_projection);
    for (uint32_t i = 0u; i < layout.blend_weight_count; ++i) {
        float world[16];
        if (!read_transform(device, D3D_TRANSFORM_WORLD + i + 1u, world)) return false;
        multiply_transform(world, view_projection, draw->blend_transforms[i]);
    }
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

/* CPU span of a supported texture. Movie BGRA uses a row pitch; other
   uncompressed surfaces are Morton-ordered, and DXT blocks upload as-is. */
static bool swizzled_byte_count(
    const RecompD3dTextureDesc *desc,
    uint32_t *out)
{
    uint32_t blocks_wide;
    uint32_t blocks_high;

    if (desc->linear && desc->format_byte == 0x12u) {
        uint64_t row_bytes = (uint64_t)desc->width * 4u;
        uint64_t bytes;

        if (desc->depth || desc->bits_per_pixel != 32u ||
            desc->width == 0u || desc->height == 0u ||
            desc->pitch < row_bytes) return false;
        bytes = (uint64_t)(desc->height - 1u) * desc->pitch + row_bytes;
        if (bytes > UINT32_MAX) return false;
        *out = (uint32_t)bytes;
        return true;
    }
    if (desc->linear || desc->width == 0u || desc->height == 0u) {
        return false;
    }
    if (desc->format_byte == RECOMP_D3D_TEXTURE_FORMAT_P8) {
        uint64_t bytes = (uint64_t)desc->width * desc->height;
        if (desc->bits_per_pixel != 8u || bytes > UINT32_MAX) return false;
        *out = (uint32_t)bytes;
        return true;
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

static bool attach_stage0_palette(RecompD3dPresenterDrawCommand *draw)
{
    const uint8_t *bytes = guest_span(D3D_DEVICE_GLOBAL, 4u);
    uint32_t device, palette, common, data;

    if (bytes == NULL) return false;
    memcpy(&device, bytes, sizeof device);
    /* Original SetPalette (001E45D0) stores stage 0 at device + 0xB48. */
    if (device == 0u || (uint64_t)device + 0xb48u > UINT32_MAX) return false;
    bytes = guest_span(device + 0xb48u, 4u);
    if (bytes == NULL) return false;
    memcpy(&palette, bytes, sizeof palette);
    if (palette == 0u) return false;
    bytes = guest_span(palette, 8u);
    if (bytes == NULL) return false;
    memcpy(&common, bytes, sizeof common);
    memcpy(&data, bytes + 4u, sizeof data);
    /* The observed UI palettes have 256 entries (size code zero). */
    if ((common & D3D_RESOURCE_TYPE_MASK) != 0x00030000u ||
        (common >> 30u) != 0u || data == 0u) return false;
    bytes = guest_span(data, 1024u);
    if (bytes == NULL) return false;
    draw->palette_bytes = bytes;
    draw->palette_byte_count = 1024u;
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
    draw->texture = *desc;
    draw->has_texture = true;
    if (!swizzled_byte_count(desc, &byte_count)) {
        /* A render target may have host-owned pixels without a CPU upload. */
        ++draw_unsupported_formats[desc->format_byte & 0xffu];
        return;
    }
    bytes = guest_span(desc->data, byte_count);
    if (bytes == NULL) {
        ++draw_unmapped_formats[desc->format_byte & 0xffu];
        return;
    }
    if (desc->format_byte == RECOMP_D3D_TEXTURE_FORMAT_P8 &&
        !attach_stage0_palette(draw)) {
        ++draw_unsupported_formats[RECOMP_D3D_TEXTURE_FORMAT_P8];
        return;
    }
    draw->texture_bytes = bytes;
    draw->texture_byte_count = byte_count;
}

static bool read_texture_factor_selector(uint32_t selector[4])
{
    const uint8_t *state = guest_span(0x001f29b8u, 28u);

    if (state == NULL) return false;
    for (uint32_t i = 0u; i < 4u; ++i) {
        memcpy(&selector[i], state + i * 8u, sizeof selector[i]);
    }
    return true;
}

static bool read_stage1_arguments(uint32_t arguments[6])
{
    const uint8_t *state = guest_span(0x001f2a38u, 32u);
    const uint32_t offsets[] = {0u, 8u, 12u, 16u, 24u, 28u};

    if (state == NULL) return false;
    for (uint32_t i = 0u; i < 6u; ++i) {
        memcpy(&arguments[i], state + offsets[i], sizeof arguments[i]);
    }
    return true;
}

static void attach_material_state(
    uint32_t device, const uint32_t selector[4], RecompD3dPresenterDrawCommand *draw)
{
    RecompD3dVertexLayout layout;
    const uint8_t *lighting = guest_span(0x001f2d20u, 16u);
    const uint8_t *arguments = guest_span(0x001f29c4u, 20u);
    const uint8_t *material = device != 0u && (uint64_t)device + 0xac0u <= UINT32_MAX
        ? guest_span(device + 0xabcu, 4u) : NULL;
    uint32_t enabled, color_vertex, color_argument, alpha_argument;
    float alpha;

    if (lighting == NULL || arguments == NULL || material == NULL ||
        !recomp_d3d_fvf_layout(draw->fvf, &layout) ||
        layout.diffuse_offset != RECOMP_D3D_FVF_ABSENT) return;
    memcpy(&enabled, lighting, 4u);
    memcpy(&color_vertex, lighting + 12u, 4u);
    memcpy(&color_argument, arguments, 4u);
    memcpy(&alpha_argument, arguments + 16u, 4u);
    memcpy(&alpha, material, 4u);
    if (enabled == 0u || color_vertex != 0u) return;
    RecompD3dMaterialAlphaMode mode = recomp_d3d_texture_material_alpha_mode(
        selector[0], selector[1], color_argument,
        selector[2], selector[3], alpha_argument);
    if (isfinite(alpha) &&
        mode != RECOMP_D3D_MATERIAL_ALPHA_NONE) {
        draw->material_alpha_mode = mode;
        draw->material_alpha = alpha;
    }
    if (selector[0] == 4u && selector[1] == 2u && color_argument == 0u) {
        const uint8_t *two_sided_bytes = guest_span(0x001f2dbcu, 4u);
        const uint8_t *ambient_bytes = guest_span(0x001f2d54u, 4u);
        const uint8_t *light_bytes = guest_span(device + 0x398u, 4u);
        const uint8_t *emissive_bytes = (uint64_t)device + 0xaecu <= UINT32_MAX
            ? guest_span(device + 0xae0u, 12u) : NULL;
        uint32_t two_sided, ambient, active_light_head;
        float emissive[3];

        if (two_sided_bytes == NULL || ambient_bytes == NULL ||
            light_bytes == NULL || emissive_bytes == NULL) return;
        memcpy(&two_sided, two_sided_bytes, sizeof two_sided);
        if (two_sided != 0u) return;
        memcpy(&ambient, ambient_bytes, sizeof ambient);
        memcpy(&active_light_head, light_bytes, sizeof active_light_head);
        memcpy(emissive, emissive_bytes, sizeof emissive);
        draw->zero_diffuse_rgb = recomp_d3d_diffuse_rgb_is_zero(
            ambient, active_light_head, emissive);
    }
}

static bool same_texture_storage(
    const RecompD3dTextureDesc *a, const RecompD3dTextureDesc *b)
{
    return a->data == b->data && a->format_byte == b->format_byte &&
        a->bits_per_pixel == b->bits_per_pixel && a->linear == b->linear &&
        a->width == b->width && a->height == b->height && a->pitch == b->pitch;
}

static bool reject_four_tap_filter(
    const char *field, uint32_t slot, uint32_t actual, uint32_t expected)
{
    static const char *fields[8];
    static uint32_t slots[8];
    static uint32_t reported;

    for (uint32_t i = 0u; i < reported; ++i) {
        if (slots[i] == slot && strcmp(fields[i], field) == 0) return false;
    }
    if (reported < 8u) {
        fields[reported] = field;
        slots[reported++] = slot;
        fprintf(stderr, "recomp d3d filter: rejected %s[0x%08X] actual=0x%08X expected=0x%08X\n",
            field, slot, actual, expected);
    }
    return false;
}

static bool attach_four_tap_filter(
    uint32_t device, RecompD3dPresenterDrawCommand *draw)
{
    /* SetPixelShader copies structural state to the guest shadow. Simple
       overrides are owned by the host model and leave that shadow unchanged. */
    static const uint32_t expected[][2] = {
        {0x08u, 0xdc30dd30u},
        {0x20u, 0x0000000cu}, {0x24u, 0x00001c80u},
        {0x68u, 0x00010c00u}, {0x6cu, 0x00010d00u}, {0x70u, 0x00030c00u},
        {0x90u, 0xcc20cd20u},
        {0xb4u, 0x00010c00u}, {0xb8u, 0x00010d00u}, {0xbcu, 0x00030c00u},
        {0xd4u, 0x00011103u}, {0xd8u, 0x00008421u},
    };
    static const uint32_t simple_expected[][2] = {
        {0x40260u, 0xd1d8d2d9u}, {0x40264u, 0xd1dad2dbu},
        {0x40a60u, 0x40404040u}, {0x40a64u, 0x40404040u},
        {0x40a80u, 0x40404040u}, {0x40a84u, 0x40404040u},
        {0x40ac0u, 0xc1c8c2c9u}, {0x40ac4u, 0xc1cac2cbu},
    };
    static const uint32_t sampler_expected[][2] = {
        {0x00u, 3u}, {0x04u, 3u}, {0x0cu, 2u}, {0x10u, 2u}, {0x14u, 1u},
        {0x1cu, 0u}, {0x24u, 0u},
        {0x28u, 0u}, {0x2cu, 0u}, {0x54u, 0u},
    };
    const uint8_t *shader = (uint64_t)device + 0x374u <= UINT32_MAX
        ? guest_span(device + 0x370u, 4u) : NULL;
    const uint8_t *state = guest_span(0x001f2b88u, 0xdcu);
    const RecompD3dTextureDesc *source = recomp_d3d_texture_adapter_stage(0u);
    uint32_t value;

    if (shader == NULL) return reject_four_tap_filter("shader-memory", device, 0u, 1u);
    if (state == NULL) return reject_four_tap_filter("state-memory", 0x001f2b88u, 0u, 1u);
    if (source == NULL) return reject_four_tap_filter("source-missing", 0u, 0u, 1u);
    if (source->data == 0u || source->width == 0u || source->height == 0u || source->depth) {
        return reject_four_tap_filter("source-invalid", source->data, source->depth, 0u);
    }
    if (source->bits_per_pixel != 32u ||
        !((source->format_byte == 0x12u && source->linear) ||
          (source->format_byte == 0x06u && !source->linear))) {
        return reject_four_tap_filter("source-format", source->linear,
            source->format_byte, source->linear ? 0x12u : 0x06u);
    }
    memcpy(&value, shader, sizeof value);
    if (value == 0u) return reject_four_tap_filter("pixel-shader", device + 0x370u, 0u, 1u);
    for (uint32_t i = 0u; i < sizeof expected / sizeof expected[0]; ++i) {
        memcpy(&value, state + expected[i][0], sizeof value);
        if (value != expected[i][1]) {
            return reject_four_tap_filter("shader", 0x001f2b88u + expected[i][0], value, expected[i][1]);
        }
    }
    for (uint32_t i = 0u; i < sizeof simple_expected / sizeof simple_expected[0]; ++i) {
        if (!recomp_d3d_get_simple_render_state(recomp_d3d_render_state_adapter_model(),
                simple_expected[i][0], &value)) {
            return reject_four_tap_filter("simple-missing", simple_expected[i][0], 0u, simple_expected[i][1]);
        }
        if (value != simple_expected[i][1]) {
            return reject_four_tap_filter("simple", simple_expected[i][0], value, simple_expected[i][1]);
        }
    }
    for (uint32_t stage = 0u; stage < 4u; ++stage) {
        const uint8_t *sampler = guest_span(0x001f2988u + stage * 0x80u, 0x74u);
        const RecompD3dTextureDesc *texture = recomp_d3d_texture_adapter_stage(stage);
        uint32_t resource = recomp_d3d_texture_adapter_model()->textures[stage];
        const uint8_t *resource_bytes = resource != 0u ? guest_span(resource, 20u) : NULL;
        float lod_bias;

        if (resource_bytes == NULL) return reject_four_tap_filter("texture-memory", stage, resource, 1u);
        memcpy(&value, resource_bytes + 12u, sizeof value);
        if (((value >> 16u) & 15u) != 1u) {
            return reject_four_tap_filter("texture-mip-levels", stage, (value >> 16u) & 15u, 1u);
        }
        if (sampler == NULL) return reject_four_tap_filter("sampler-memory", stage, 0u, 1u);
        for (uint32_t i = 0u; i < sizeof sampler_expected / sizeof sampler_expected[0]; ++i) {
            memcpy(&value, sampler + sampler_expected[i][0], sizeof value);
            if (value != sampler_expected[i][1]) {
                return reject_four_tap_filter("sampler", 0x001f2988u + stage * 0x80u + sampler_expected[i][0],
                    value, sampler_expected[i][1]);
            }
        }
        /* The filter inherits LOD bias and the anisotropy limit. A single
           mip level makes finite bias inert; linear MIN/MAG ignore the limit. */
        memcpy(&lod_bias, sampler + 0x18u, sizeof lod_bias);
        if (!isfinite(lod_bias)) {
            memcpy(&value, sampler + 0x18u, sizeof value);
            return reject_four_tap_filter("nonfinite-lod-bias", stage, value, 0u);
        }
        memcpy(&value, sampler + 0x70u, sizeof value);
        if (value != stage) return reject_four_tap_filter("texcoord-index", stage, value, stage);
        if (texture == NULL) return reject_four_tap_filter("texture-missing", stage, 0u, 1u);
        if (texture->depth) return reject_four_tap_filter("texture-depth", stage, 1u, 0u);
        if (!same_texture_storage(source, texture)) {
            const uint32_t actual[] = {texture->data, texture->format_byte, texture->bits_per_pixel,
                texture->linear, texture->width, texture->height, texture->pitch};
            const uint32_t wanted[] = {source->data, source->format_byte, source->bits_per_pixel,
                source->linear, source->width, source->height, source->pitch};
            const char *fields[] = {"texture-data", "texture-format", "texture-bpp",
                "texture-linear", "texture-width", "texture-height", "texture-pitch"};
            for (uint32_t i = 0u; i < 7u; ++i) {
                if (actual[i] != wanted[i]) return reject_four_tap_filter(fields[i], stage, actual[i], wanted[i]);
            }
        }
    }
    draw->four_tap_filter = true;
    return true;
}

static void attach_backbuffer_texture(
    uint32_t device, RecompD3dPresenterDrawCommand *draw)
{
    const uint8_t *bytes = (uint64_t)device + D3D_BACK_BUFFER_OFFSET + 4u <= UINT32_MAX
        ? guest_span(device + D3D_BACK_BUFFER_OFFSET, 4u) : NULL;
    RecompD3dTextureDesc backbuffer;
    uint32_t resource, format;

    if (!draw->has_texture || bytes == NULL) return;
    memcpy(&resource, bytes, sizeof resource);
    bytes = resource != 0u ? guest_span(resource, 20u) : NULL;
    if (bytes == NULL) return;
    memcpy(&format, bytes + 12u, sizeof format);
    if (guest_span(0x001f16b8u + ((format >> 8u) & 0xffu), 1u) == NULL) return;
    if (recomp_d3d_texture_adapter_describe(resource, &backbuffer) &&
        backbuffer.data != 0u && !backbuffer.depth &&
        same_texture_storage(&draw->texture, &backbuffer)) {
        draw->texture_is_backbuffer = true;
    }
}

static bool attach_draw_state(uint32_t device, RecompD3dPresenterDrawCommand *draw)
{
    uint32_t selector[4];
    bool has_selector = read_texture_factor_selector(selector);

    if (draw->fvf == 0x404u && !attach_four_tap_filter(device, draw)) return false;

    recomp_d3d_depth_state(recomp_d3d_render_state_adapter_model(), &draw->depth);
    recomp_d3d_blend_state(recomp_d3d_render_state_adapter_model(), &draw->blend);
    draw->texture_factor = recomp_d3d_render_state_adapter_model()->texture_factor;
    draw->use_texture_factor = has_selector && recomp_d3d_texture_factor_selected(
        selector[0], selector[1], selector[2], selector[3]);
    if (has_selector) attach_material_state(device, selector, draw);
    if (!draw->use_texture_factor &&
        draw->material_alpha_mode == RECOMP_D3D_MATERIAL_ALPHA_MODULATE_TEXTURE) {
        uint32_t stage1_arguments[6], next_color_op;
        const uint8_t *next = guest_span(0x001f2ab8u, 4u);

        if (next != NULL && read_stage1_arguments(stage1_arguments)) {
            memcpy(&next_color_op, next, sizeof next_color_op);
            draw->modulate_texture_factor =
                recomp_d3d_texture_factor_modulate_selected(stage1_arguments, next_color_op);
        }
    }
    if (!recomp_d3d_frame_adapter_target(&draw->target)) return false;
    attach_stage0_texture(draw);
    attach_backbuffer_texture(device, draw);
    return true;
}

/* One opt-in diagnostic present, separate from the native screenshot cadence. */
enum { CAPTURE_ROWS = 4096u, CAPTURE_INDICES = 262144u };
static struct {
    bool configured, done;
    const char *path;
    FILE *file;
    uint32_t at, rows, draws, accepted, scans, partial;
    char buffer[65536];
} draw_capture;

typedef struct CaptureBounds {
    float low[4], high[4];
    uint32_t finite, nonfinite;
} CaptureBounds;

static bool capture_open(uint32_t present)
{
    if (!draw_capture.configured) {
        const char *at = getenv("RECOMP_D3D_DRAW_CAPTURE_AT");
        char *end;
        unsigned long value;

        draw_capture.configured = true;
        draw_capture.path = getenv("RECOMP_D3D_DRAW_CAPTURE");
        if (draw_capture.path == NULL || *draw_capture.path == '\0' || at == NULL) {
            draw_capture.done = true;
            return false;
        }
        errno = 0;
        value = strtoul(at, &end, 10);
        if (errno != 0 || end == at || *end != '\0' || *at == '-' ||
            value == 0ul || value > UINT32_MAX) {
            draw_capture.done = true;
            return false;
        }
        draw_capture.at = (uint32_t)value;
    }
    if (draw_capture.done || present != draw_capture.at) {
        return false;
    }
    if (draw_capture.file == NULL) {
        draw_capture.file = fopen(draw_capture.path, "wb");
        if (draw_capture.file == NULL) {
            draw_capture.done = true;
            return false;
        }
        setvbuf(draw_capture.file, draw_capture.buffer, _IOFBF, sizeof draw_capture.buffer);
        fprintf(draw_capture.file,
            "{\"kind\":\"begin\",\"present\":%u,\"row_cap\":%u,\"index_cap\":%u}\n",
            present, CAPTURE_ROWS, CAPTURE_INDICES);
    }
    return true;
}

static bool capture_word(uint32_t base, uint32_t offset, uint32_t *value)
{
    const uint8_t *bytes = base != 0u && (uint64_t)base + offset <= UINT32_MAX
        ? guest_span(base + offset, 4u) : NULL;
    if (bytes == NULL) {
        return false;
    }
    memcpy(value, bytes, 4u);
    return true;
}

static void capture_word_json(uint32_t base, uint32_t offset)
{
    uint32_t value;
    if (capture_word(base, offset, &value)) {
        fprintf(draw_capture.file, "%u", value);
    } else {
        fputs("null", draw_capture.file);
    }
}

static void capture_floats(const float *values, uint32_t count)
{
    fputc('[', draw_capture.file);
    for (uint32_t i = 0u; i < count; ++i) {
        if (i != 0u) fputc(',', draw_capture.file);
        if (isfinite(values[i])) fprintf(draw_capture.file, "%.9g", values[i]);
        else fputs("null", draw_capture.file);
    }
    fputc(']', draw_capture.file);
}

static void capture_bounds_add(CaptureBounds *bounds, const float *values, uint32_t count)
{
    for (uint32_t i = 0u; i < count; ++i) {
        if (!isfinite(values[i])) {
            ++bounds->nonfinite;
            return;
        }
    }
    for (uint32_t i = 0u; i < count; ++i) {
        if (bounds->finite == 0u || values[i] < bounds->low[i]) bounds->low[i] = values[i];
        if (bounds->finite == 0u || values[i] > bounds->high[i]) bounds->high[i] = values[i];
    }
    ++bounds->finite;
}

static void capture_bounds_json(const char *name, const CaptureBounds *bounds, uint32_t count)
{
    fprintf(draw_capture.file, ",\"%s\":{\"finite\":%u,\"nonfinite\":%u,\"min\":",
        name, bounds->finite, bounds->nonfinite);
    if (bounds->finite != 0u) capture_floats(bounds->low, count);
    else fputs("null", draw_capture.file);
    fputs(",\"max\":", draw_capture.file);
    if (bounds->finite != 0u) capture_floats(bounds->high, count);
    else fputs("null", draw_capture.file);
    fputc('}', draw_capture.file);
}

static void capture_draw(
    uint32_t device, uint32_t primitive, uint32_t count, uint32_t indices,
    const RecompD3dDrawResult *result, const RecompD3dPresenterDrawCommand *draw,
    const char *outcome)
{
    const uint32_t present = recomp_d3d_frame_adapter_swap_counter() + 1u;
    if (!capture_open(present)) return;

    const RecompD3dTextureDesc *texture;
    RecompD3dVertexLayout layout;
    CaptureBounds xyz = {0}, clip = {0}, normal = {0}, uv = {0};
    uint32_t declaration = 0u, scanned = 0u, low = UINT32_MAX, high = 0u;
    uint32_t unmapped = 0u, bad_index = UINT32_MAX, nonpositive_w = 0u;
    bool known;

    ++draw_capture.draws;
    if (strcmp(outcome, "accepted") == 0) ++draw_capture.accepted;
    if (draw_capture.rows == CAPTURE_ROWS) return;
    ++draw_capture.rows;
    known = draw != NULL && recomp_d3d_fvf_layout(draw->fvf, &layout) &&
        layout.stride == draw->vertex_stride;
    for (; scanned < count && draw_capture.scans < CAPTURE_INDICES; ++scanned) {
        uint64_t address = (uint64_t)indices + 2u * (uint64_t)scanned;
        const uint8_t *bytes = address <= UINT32_MAX ? guest_span((uint32_t)address, 2u) : NULL;
        uint16_t index;
        ++draw_capture.scans;
        if (bytes == NULL) break;
        memcpy(&index, bytes, 2u);
        if (index < low) low = index;
        if (index > high) high = index;
        if (known) {
            float p[3], c[4];
            address = (uint64_t)result->plan.vertex_data + (uint64_t)index * draw->vertex_stride;
            bytes = address <= UINT32_MAX ? guest_span((uint32_t)address, draw->vertex_stride) : NULL;
            if (bytes == NULL) {
                ++unmapped;
                if (bad_index == UINT32_MAX) bad_index = index;
                continue;
            }
            memcpy(p, bytes + layout.position_offset, sizeof p);
            capture_bounds_add(&xyz, p, 3u);
            if ((!isfinite(p[0]) || !isfinite(p[1]) || !isfinite(p[2])) && bad_index == UINT32_MAX)
                bad_index = index;
            for (uint32_t j = 0u; j < 4u; ++j)
                c[j] = p[0] * draw->transform[j] + p[1] * draw->transform[4u+j] +
                    p[2] * draw->transform[8u+j] + draw->transform[12u+j];
            if (draw->blend_weight_count != 0u) {
                float weights[3], remainder = 1.0f;
                memcpy(weights, bytes + 12u, draw->blend_weight_count * sizeof(float));
                for (uint32_t k = 0u; k < draw->blend_weight_count; ++k) remainder -= weights[k];
                for (uint32_t j = 0u; j < 4u; ++j) {
                    c[j] *= weights[0];
                    for (uint32_t k = 1u; k <= draw->blend_weight_count; ++k) {
                        const float *m = draw->blend_transforms[k - 1u];
                        float weight = k == draw->blend_weight_count ? remainder : weights[k];
                        c[j] += weight * (p[0]*m[j] + p[1]*m[4u+j] + p[2]*m[8u+j] + m[12u+j]);
                    }
                }
            }
            capture_bounds_add(&clip, c, 4u);
            if (isfinite(c[3]) && c[3] <= 0.0f) ++nonpositive_w;
            if (layout.normal_offset != RECOMP_D3D_FVF_ABSENT) {
                memcpy(p, bytes + layout.normal_offset, sizeof p);
                capture_bounds_add(&normal, p, 3u);
            }
            if (layout.texcoord_offset != RECOMP_D3D_FVF_ABSENT) {
                memcpy(p, bytes + layout.texcoord_offset, 2u * sizeof(float));
                capture_bounds_add(&uv, p, 2u);
            }
        }
    }
    if (scanned != count) ++draw_capture.partial;
    fprintf(draw_capture.file,
        "{\"kind\":\"draw\",\"present\":%u,\"ordinal\":%u,\"outcome\":\"%s\","
        "\"device\":%u,\"raw_shader\":", present, draw_capture.draws, outcome, device);
    capture_word_json(device, D3D_VERTEX_SHADER_HANDLE_OFFSET);
    fprintf(draw_capture.file, ",\"fvf\":%u,\"base_vertex\":", draw_state.fvf);
    capture_word_json(device, 0x1cu);
    const RecompD3dRenderStateModel *render_state = recomp_d3d_render_state_adapter_model();
    uint32_t color_mask;
    fputs(",\"color_mask\":", draw_capture.file);
    if (recomp_d3d_get_simple_render_state(render_state, 0x40358u, &color_mask))
        fprintf(draw_capture.file, "%u", color_mask);
    else fputs("null", draw_capture.file);
    fprintf(draw_capture.file, ",\"stencil_enable\":%u,\"color_mask_shadow\":",
        render_state->stencil_enable);
    capture_word_json(0x001f2c94u, 0u);
    fputs(",\"stencil_enable_shadow\":", draw_capture.file);
    capture_word_json(0x001f2dc8u, 0u);
    RecompD3dDepthState depth;
    recomp_d3d_depth_state(render_state, &depth);
    fprintf(draw_capture.file,
        ",\"stencil_func\":%u,\"stencil_ref\":%u,\"stencil_read_mask\":%u"
        ",\"stencil_write_mask\":%u,\"stencil_fail\":%u,\"stencil_zfail\":%u"
        ",\"stencil_pass\":%u,\"texture_factor\":%u,\"use_texture_factor\":",
        (unsigned)depth.stencil_func, depth.stencil_ref, depth.stencil_read_mask,
        depth.stencil_write_mask, (unsigned)depth.stencil_fail,
        (unsigned)depth.stencil_zfail, (unsigned)depth.stencil_pass,
        render_state->texture_factor);
    fputs(draw != NULL ? (draw->use_texture_factor ? "true" : "false") : "null",
        draw_capture.file);
    uint32_t selector[4];
    fputs(",\"texture_factor_selector\":", draw_capture.file);
    if (read_texture_factor_selector(selector)) {
        fprintf(draw_capture.file, "[%u,%u,%u,%u]",
            selector[0], selector[1], selector[2], selector[3]);
    } else fputs("null", draw_capture.file);
    fprintf(draw_capture.file, ",\"cull_mode\":%u,\"cull_updates\":%u,\"cull_shadow\":",
        render_state->cull_mode, render_state->cull_mode_update_count);
    capture_word_json(0x001f2dd4u, 0u);
    fputs(",\"lighting\":", draw_capture.file);
    capture_word_json(0x001f2d20u, 0u);
    fputs(",\"two_sided_lighting\":", draw_capture.file);
    capture_word_json(0x001f2dbcu, 0u);
    fputs(",\"color_vertex\":", draw_capture.file);
    capture_word_json(0x001f2d2cu, 0u);
    fputs(",\"diffuse_source\":", draw_capture.file);
    capture_word_json(0x001f2d44u, 0u);
    fputs(",\"ambient\":", draw_capture.file);
    capture_word_json(0x001f2d54u, 0u);
    fputs(",\"active_light_head\":", draw_capture.file);
    capture_word_json(device, 0x398u);
    fputs(",\"material_emissive\":", draw_capture.file);
    const uint8_t *emissive = device != 0u && (uint64_t)device + 0xaf0u <= UINT32_MAX
        ? guest_span(device + 0xae0u, 16u) : NULL;
    if (emissive != NULL) {
        float rgba[4];
        memcpy(rgba, emissive, sizeof rgba);
        capture_floats(rgba, 4u);
    } else fputs("null", draw_capture.file);
    fputs(",\"zero_diffuse_rgb\":", draw_capture.file);
    fputs(draw != NULL ? (draw->zero_diffuse_rgb ? "true" : "false") : "null",
        draw_capture.file);
    fputs(",\"pixel_shader\":", draw_capture.file);
    capture_word_json(device, 0x370u);
    fputs(",\"material_diffuse\":", draw_capture.file);
    const uint8_t *material = device != 0u && (uint64_t)device + 0xac0u <= UINT32_MAX
        ? guest_span(device + 0xab0u, 16u) : NULL;
    if (material != NULL) {
        float diffuse[4];
        memcpy(diffuse, material, sizeof diffuse);
        capture_floats(diffuse, 4u);
    } else fputs("null", draw_capture.file);
    fputs(",\"stage0_args\":[", draw_capture.file);
    const uint32_t argument_offsets[] = {0u, 8u, 12u, 16u, 24u, 28u};
    for (uint32_t i = 0u; i < 6u; ++i) {
        if (i != 0u) fputc(',', draw_capture.file);
        capture_word_json(0x001f29b8u, argument_offsets[i]);
    }
    fputs("],\"stage1_args\":", draw_capture.file);
    uint32_t stage1_arguments[6];
    if (read_stage1_arguments(stage1_arguments)) {
        fprintf(draw_capture.file, "[%u,%u,%u,%u,%u,%u]",
            stage1_arguments[0], stage1_arguments[1], stage1_arguments[2],
            stage1_arguments[3], stage1_arguments[4], stage1_arguments[5]);
    } else fputs("null", draw_capture.file);
    fputs(",\"stage2_color_op\":", draw_capture.file);
    capture_word_json(0x001f2ab8u, 0u);
    fprintf(draw_capture.file, ",\"alpha_test\":[%u,%u,%u],\"depth\":[%u,%u,%u]",
        depth.alpha_test_enable, (unsigned)depth.alpha_func, depth.alpha_ref,
        depth.depth_test_enable, depth.depth_write_enable, (unsigned)depth.depth_func);
    RecompD3dBlendState blend;
    recomp_d3d_blend_state(render_state, &blend);
    fprintf(draw_capture.file,
        ",\"alpha_blend\":[%u,%u,%u,%u],\"material_alpha_mode\":%u,\"modulate_texture_factor\":%s",
        blend.blend_enable, (unsigned)blend.src_factor, (unsigned)blend.dst_factor, (unsigned)blend.op,
        draw != NULL ? (unsigned)draw->material_alpha_mode : RECOMP_D3D_MATERIAL_ALPHA_NONE,
        draw != NULL && draw->modulate_texture_factor ? "true" : "false");
    fputs(",\"blend_mode\":", draw_capture.file);
    capture_word_json(D3D_VERTEX_BLEND_SHADOW, 0u);
    fputs(",\"stream0\":[", draw_capture.file);
    for (uint32_t i = 0u; i < 3u; ++i) {
        if (i != 0u) fputc(',', draw_capture.file);
        capture_word_json(D3D_STREAM0_STRIDE, i * 4u);
    }
    fprintf(draw_capture.file, "],\"vertex_data\":%u,\"declaration\":", draw_state.stream0.vertex_data);
    capture_word_json(device, 0x380u);
    capture_word(device, 0x380u, &declaration);
    fputs(",\"position_decl\":[", draw_capture.file);
    for (uint32_t i = 0u; i < 3u; ++i) {
        if (i != 0u) fputc(',', draw_capture.file);
        capture_word_json(declaration, 0x14u + i * 4u);
    }
    fprintf(draw_capture.file,
        "],\"primitive\":%u,\"index_data\":%u,\"index_count\":%u,\"index_scanned\":%u,"
        "\"indices_complete\":%s,\"index_range\":", primitive, indices, count, scanned,
        scanned == count ? "true" : "false");
    if (scanned != 0u) fprintf(draw_capture.file, "[%u,%u]", low, high);
    else fputs("null", draw_capture.file);
    fprintf(draw_capture.file, ",\"plan_error\":%u,\"position_status\":\"%s\",\"unmapped_vertices\":%u,\"bad_index\":",
        (unsigned)result->error, known ? "decoded_xyz" : "unavailable", unmapped);
    if (bad_index != UINT32_MAX) fprintf(draw_capture.file, "%u", bad_index);
    else fputs("null", draw_capture.file);
    fprintf(draw_capture.file, ",\"plan\":[%u,%u,%u],\"offscreen\":",
        result->plan.vertex_data, result->plan.vertex_stride, result->plan.triangle_count);
    if (draw != NULL) fputs(draw->target.offscreen ? "true" : "false", draw_capture.file);
    else fputs("null", draw_capture.file);
    fprintf(draw_capture.file, ",\"normal_offset\":%u,\"uv_offset\":%u,\"w_nonpositive\":%u,\"wvp\":",
        known ? layout.normal_offset : RECOMP_D3D_FVF_ABSENT,
        known ? layout.texcoord_offset : RECOMP_D3D_FVF_ABSENT, nonpositive_w);
    if (known) capture_floats(draw->transform, 16u);
    else fputs("null", draw_capture.file);
    capture_bounds_json("xyz", &xyz, 3u);
    capture_bounds_json("clip", &clip, 4u);
    capture_bounds_json("normal", &normal, 3u);
    capture_bounds_json("uv", &uv, 2u);
    if ((draw_state.fvf & 0xfu) == 6u || (draw_state.fvf & 0xfu) == 8u ||
        (draw_state.fvf & 0xfu) == 10u) {
        fputs(",\"worlds\":[", draw_capture.file);
        for (uint32_t i = 0u; i < 4u; ++i) {
            float world[16];
            if (i != 0u) fputc(',', draw_capture.file);
            if (read_transform(device, D3D_TRANSFORM_WORLD + i, world)) capture_floats(world, 16u);
            else fputs("null", draw_capture.file);
        }
        fputc(']', draw_capture.file);
    }
    fputs(",\"texture0_object\":", draw_capture.file);
    capture_word_json(device, 0xb38u);
    texture = recomp_d3d_texture_adapter_stage(0u);
    fputs(",\"texture0\":", draw_capture.file);
    if (texture != NULL) fprintf(draw_capture.file,
        "{\"data\":%u,\"format\":%u,\"width\":%u,\"height\":%u,\"linear\":%s}",
        texture->data, texture->format_byte, texture->width, texture->height,
        texture->linear ? "true" : "false");
    else fputs("null", draw_capture.file);
    fprintf(draw_capture.file, ",\"texture_attached\":%s,\"targets\":[",
        draw != NULL && draw->has_texture ? "true" : "false");
    capture_word_json(device, D3D_CURRENT_RENDER_TARGET_OFFSET);
    fputc(',', draw_capture.file); capture_word_json(device, D3D_BACK_BUFFER_OFFSET);
    fputc(',', draw_capture.file); capture_word_json(device, 0x21b8u);
    fputs("]}\n", draw_capture.file);
}

void recomp_d3d_draw_adapter_capture_present(uint32_t present, uint32_t presenter_error)
{
    if (!capture_open(present)) return;
    fprintf(draw_capture.file,
        "{\"kind\":\"end\",\"present\":%u,\"present_error\":%u,\"rows\":%u,"
        "\"accepted\":%u,\"declined\":%u,\"rows_dropped\":%u,\"index_scans\":%u,\"partial_bounds\":%u}\n",
        present, presenter_error, draw_capture.rows, draw_capture.accepted,
        draw_capture.draws - draw_capture.accepted, draw_capture.draws - draw_capture.rows,
        draw_capture.scans, draw_capture.partial);
    fclose(draw_capture.file);
    draw_capture.file = NULL;
    draw_capture.done = true;
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
    const char *decline = NULL;
    const RecompD3dPresenterDrawCommand *capture_command = NULL;

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
        decline = "plan";
        goto finished;
    }
    if (recomp_d3d_fvf_stride(result.plan.fvf) != result.plan.vertex_stride) {
        decline = "fvf";
        goto finished;
    }

    index_bytes = guest_span(result.plan.index_data, result.plan.index_bytes);
    if (index_bytes == NULL) {
        decline = "indices";
        goto finished;
    }
    vertex_count = largest_index(index_bytes, result.plan.index_count) + 1u;
    vertex_span = recomp_d3d_draw_vertex_bytes(
        result.plan.vertex_stride, vertex_count - 1u);
    vertex_bytes = guest_span(result.plan.vertex_data, vertex_span);
    if (vertex_bytes == NULL) {
        decline = "vertices";
        goto finished;
    }
    if (device == 0u || !compose_world_view_projection(device, transform)) {
        decline = "transform";
        goto finished;
    }

    memset(&command, 0, sizeof command);
    command.type = RECOMP_D3D_PRESENTER_COMMAND_DRAW;
    /* Round 21 probe. FUN_001221e0 and FUN_001225f0 are render-to-texture
       passes: they call GetRenderTarget2, bind a texture surface with
       SetRenderTarget, draw, then restore. Attachment snapshots now route
       these draws to host render targets. An earlier probe sampled
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
    if (!compose_blend_transforms(device, &command.data.draw)) {
        decline = "vertex-blend";
        goto finished;
    }
    if (!attach_draw_state(device, &command.data.draw)) {
        decline = "render-target";
        goto finished;
    }
    capture_command = &command.data.draw;

    if (recomp_d3d_presenter_submit(
            recomp_d3d_frame_adapter_presenter(), &command) !=
        RECOMP_D3D_PRESENTER_OK) {
        decline = "presenter";
        goto finished;
    }
    ++draw_submitted;
    record_fvf(result.plan.fvf);
finished:
    if (decline != NULL) report_decline(decline);
    capture_draw(device, primitive_type, index_count, index_data, &result,
        capture_command, decline != NULL ? decline : "accepted");
}

static void recomp_d3d_draw_vertices_up_adapter(void)
{
    const uint32_t entry_esp = recomp_runtime.registers.esp;
    const uint32_t primitive = stack_argument(entry_esp, 0u);
    const uint32_t count = stack_argument(entry_esp, 1u);
    const uint32_t vertices = stack_argument(entry_esp, 2u);
    const uint32_t stride = stack_argument(entry_esp, 3u);
    static const uint16_t indices[4] = {0u, 1u, 2u, 3u};
    static uint32_t reported;
    RecompD3dPresenterCommand command = {0};
    const uint8_t *device_bytes, *vertex_bytes;
    uint32_t device, fvf;
    const char *decline = NULL;

    /* Preserve the driver's UP state, push-buffer/fence work, and RET 0x10. */
    sub_001E7750();
    if (primitive != RECOMP_D3D_PT_TRIANGLESTRIP || count != 4u) {
        decline = "up-shape";
        goto finished;
    }
    device = *recomp_memory_u32(D3D_DEVICE_GLOBAL);
    device_bytes = device != 0u ? guest_span(device, 0x388u) : NULL;
    if (device_bytes == NULL) {
        decline = "up-device";
        goto finished;
    }
    memcpy(&fvf, device_bytes + D3D_VERTEX_SHADER_HANDLE_OFFSET, sizeof fvf);
    if (!((fvf == 0x104u && stride == 24u) ||
          (fvf == 0x144u && stride == 28u) ||
          (fvf == 0x404u && stride == 48u))) {
        decline = "up-fvf";
        goto finished;
    }
    vertex_bytes = vertices != 0u ? guest_span(vertices, count * stride) : NULL;
    if (vertex_bytes == NULL) {
        decline = "up-vertices";
        goto finished;
    }
    for (uint32_t i = 0u; i < 4u; ++i) {
        float position[4];
        memcpy(position, vertex_bytes + i * stride, sizeof position);
        if (!isfinite(position[0]) || !isfinite(position[1]) ||
            !isfinite(position[2]) || !isfinite(position[3]) || position[3] <= 0.0f) {
            decline = "up-position";
            goto finished;
        }
    }
    command.type = RECOMP_D3D_PRESENTER_COMMAND_DRAW;
    command.data.draw.primitive_type = primitive;
    command.data.draw.index_count = 4u;
    command.data.draw.triangle_count = 2u;
    command.data.draw.vertex_count = 4u;
    command.data.draw.vertex_stride = stride;
    command.data.draw.fvf = fvf;
    command.data.draw.vertex_bytes = vertex_bytes;
    command.data.draw.index_bytes = indices;
    /* XYZRHW is already in screen space; the presenter reverses its viewport. */
    if (!attach_draw_state(device, &command.data.draw)) {
        decline = "up-render-target";
        goto finished;
    }
    if (recomp_d3d_presenter_submit(recomp_d3d_frame_adapter_presenter(), &command) !=
        RECOMP_D3D_PRESENTER_OK) {
        decline = "up-presenter";
        goto finished;
    }
    ++draw_submitted;
    record_fvf(fvf);
    if (reported < 8u) {
        ++reported;
        fprintf(stderr, "recomp d3d UP: accepted primitive=%u count=%u fvf=0x%03X "
            "stride=%u vertices=0x%08X texture=%d format=0x%02X\n",
            primitive, count, fvf, stride, vertices,
            command.data.draw.has_texture ? 1 : 0, command.data.draw.texture.format_byte);
    }
finished:
    if (decline != NULL) report_decline(decline);
}

RecompFunction recomp_d3d_draw_lookup_manual(uint32_t guest_address)
{
    if (guest_address == D3D_DEVICE_DRAW_VERTICES_UP_ADDRESS) {
        return recomp_d3d_draw_vertices_up_adapter;
    }
    return guest_address == D3D_DEVICE_DRAW_INDEXED_VERTICES_ADDRESS
        ? recomp_d3d_draw_indexed_vertices_adapter
        : NULL;
}
