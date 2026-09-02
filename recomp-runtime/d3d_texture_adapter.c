#include "d3d_texture_adapter.h"
#include "stop_report.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum {
    D3D_DEVICE_SET_TEXTURE_ADDRESS = 0x001e43f0u,
    D3D_TEXTURE_LOCK_RECT_ADDRESS = 0x001e8090u,
    D3D_DEVICE_GLOBAL = 0x001f2978u,
    D3D_STATE_DIRTY_MASK = 0x001f2984u,
    D3D_TEXTURE_SLOT_OFFSET = 0x00000b38u,
    D3D_TEXTURE_DATA_OFFSET = 0x00000004u,
    D3D_TEXTURE_FORMAT_OFFSET = 0x0000000cu,
    D3D_TEXTURE_SIZE_OFFSET = 0x00000010u,
    /* Per-format descriptor table the guest's own D3D8 indexes by format
       byte: bits 2-5 bits-per-pixel, bit 7 render target, bit 6 depth. */
    D3D_FORMAT_DESCRIPTOR_TABLE = 0x001f16b8u,
    D3D_TEXTURE_REFERENCE_STEP = 0x00080000u,
    D3D_TEXTURE_DISABLE_STATE = 0x80000000u,
    D3D_TEXTURE_DIRTY = 0x00004800u,
};

static RecompD3dTextureModel texture_model;
static RecompD3dTextureDesc stage_descs[RECOMP_D3D_TEXTURE_STAGE_COUNT];
static bool stage_desc_valid[RECOMP_D3D_TEXTURE_STAGE_COUNT];

void sub_001E8090(void);

void recomp_d3d_texture_adapter_reset(void)
{
    recomp_d3d_texture_reset(&texture_model);
    memset(stage_descs, 0, sizeof stage_descs);
    memset(stage_desc_valid, 0, sizeof stage_desc_valid);
}

const RecompD3dTextureModel *recomp_d3d_texture_adapter_model(void)
{
    return &texture_model;
}

const RecompD3dTextureDesc *recomp_d3d_texture_adapter_stage(uint32_t stage)
{
    if (stage >= RECOMP_D3D_TEXTURE_STAGE_COUNT || !stage_desc_valid[stage]) {
        return NULL;
    }
    return &stage_descs[stage];
}

void recomp_d3d_texture_adapter_report(void)
{
    const RecompD3dTextureCensus *census = &texture_model.census;
    uint32_t i;

    for (i = 0u; i < RECOMP_D3D_TEXTURE_CENSUS_SLOTS; ++i) {
        const RecompD3dTextureCensusEntry *entry = &census->entries[i];

        if (!entry->used) {
            continue;
        }
        fprintf(
            stderr,
            "recomp d3d texture: stage=%" PRIu32 " fmt=0x%02" PRIx32
            " %s %" PRIu32 "x%" PRIu32
            " bpp=%" PRIu32 " pitch=%" PRIu32 " rt=%d depth=%d binds=%"
            PRIu32 "\n",
            entry->stage,
            entry->desc.format_byte,
            entry->desc.linear ? "linear" : "swizzled",
            entry->desc.width,
            entry->desc.height,
            entry->desc.bits_per_pixel,
            entry->desc.pitch,
            entry->desc.render_target ? 1 : 0,
            entry->desc.depth ? 1 : 0,
            entry->bind_count);
    }
    if (census->overflow_count != 0u) {
        fprintf(
            stderr,
            "recomp d3d texture: census overflow=%" PRIu32 "\n",
            census->overflow_count);
    }
}

static uint32_t stack_argument(uint32_t entry_esp, uint32_t index)
{
    return *recomp_memory_u32(entry_esp + 4u + index * 4u);
}

static uint32_t texture_format_shadow(uint32_t texture)
{
    uint32_t source = *recomp_memory_u32(
        texture + D3D_TEXTURE_FORMAT_OFFSET);
    uint32_t shadow = source & 0x000020f4u;
    uint32_t format;

    if ((shadow & 0x00002000u) == 0u) {
        return shadow;
    }
    format = source & 0x0000ff00u;
    shadow &= ~0x00002000u;
    if (format >= 0x00002a00u && format <= 0x00003100u) {
        shadow |= 0x40000000u;
    }
    return shadow;
}

static void release_texture_resource(uint32_t texture)
{
    uint32_t saved_esp = recomp_runtime.registers.esp;

    recomp_runtime.registers.esp -= 4u;
    *recomp_memory_u32(recomp_runtime.registers.esp) = texture;
    recomp_runtime.registers.esp -= 4u;
    *recomp_memory_u32(recomp_runtime.registers.esp) = 0u;
    recomp_dispatch_indirect_site(
        0x001e7c90u, saved_esp, __FILE__, __LINE__);
}

static void record_texture_census(uint32_t stage, uint32_t texture)
{
    uint32_t format_dword =
        *recomp_memory_u32(texture + D3D_TEXTURE_FORMAT_OFFSET);
    uint32_t size_dword =
        *recomp_memory_u32(texture + D3D_TEXTURE_SIZE_OFFSET);
    uint32_t data = *recomp_memory_u32(texture + D3D_TEXTURE_DATA_OFFSET);
    uint32_t format_byte = (format_dword >> 8u) & 0xffu;
    uint32_t descriptor = *(const uint8_t *)(const void *)
        recomp_memory_i8(D3D_FORMAT_DESCRIPTOR_TABLE + format_byte);
    RecompD3dTextureDesc desc;

    if (recomp_d3d_texture_describe(
            format_dword, size_dword, data, descriptor, &desc)) {
        recomp_d3d_texture_census_record(&texture_model.census, &desc, stage);
        if (stage < RECOMP_D3D_TEXTURE_STAGE_COUNT) {
            stage_descs[stage] = desc;
            stage_desc_valid[stage] = true;
        }
    }
}

static void recomp_d3d_set_texture_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t stage = stack_argument(entry_esp, 0u);
    uint32_t texture = stack_argument(entry_esp, 1u);
    uint32_t device = *recomp_memory_u32(D3D_DEVICE_GLOBAL);
    uint32_t previous_texture;
    uint32_t slot_address;

    if (device == 0u || !recomp_d3d_set_texture(
            &texture_model, stage, texture)) {
        fprintf(
            stderr,
            "recomp d3d: SetTexture rejected stage 0x%08" PRIx32
            " with device 0x%08" PRIx32 "\n",
            stage,
            device);
        recomp_stop(2, "d3d-set-texture:0x%08" PRIx32, stage);
    }

    if (texture != 0u) {
        record_texture_census(stage, texture);
    } else if (stage < RECOMP_D3D_TEXTURE_STAGE_COUNT) {
        stage_desc_valid[stage] = false;
    }

    slot_address = device + D3D_TEXTURE_SLOT_OFFSET + stage * 4u;
    previous_texture = *recomp_memory_u32(slot_address);
    if (previous_texture != 0u) {
        uint32_t references = *recomp_memory_u32(previous_texture);
        uint32_t remaining = references - D3D_TEXTURE_REFERENCE_STEP;

        *recomp_memory_u32(previous_texture) = remaining;
        *recomp_memory_u32(previous_texture + 8u) = device + 0x30u;
        if ((remaining & 0x0078ffffu) == 0u) {
            release_texture_resource(previous_texture);
        }
    }

    *recomp_memory_u32(slot_address) = texture;
    if (texture == 0u) {
        *recomp_memory_u32(device + 0x0cu + stage * 4u) =
            D3D_TEXTURE_DISABLE_STATE;
        *recomp_memory_u32(D3D_STATE_DIRTY_MASK) |= D3D_TEXTURE_DIRTY;
    } else {
        uint32_t format_address = device + 0x0cu + stage * 4u;
        uint32_t previous_format = *recomp_memory_u32(format_address);
        uint32_t format = texture_format_shadow(texture);

        *recomp_memory_u32(texture) += D3D_TEXTURE_REFERENCE_STEP;
        if (previous_format != format) {
            *recomp_memory_u32(format_address) = format;
            *recomp_memory_u32(D3D_STATE_DIRTY_MASK) |= 0x00004000u;
            if (previous_texture == 0u) {
                *recomp_memory_u32(D3D_STATE_DIRTY_MASK) |= 0x00000800u;
            }
        }
    }
    recomp_runtime.registers.esp = entry_esp + 12u;
}

static void recomp_d3d_texture_lock_rect_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t locked_rect = stack_argument(entry_esp, 2u);
    uint32_t locked_address;
    uint32_t cpu_address;

    sub_001E8090();
    if (locked_rect == 0u) {
        return;
    }
    locked_address = *recomp_memory_u32(locked_rect + 4u);
    if (!recomp_d3d_texture_resolve_cpu_address(
            locked_address, &cpu_address)) {
        fprintf(
            stderr,
            "recomp d3d: Texture_LockRect rejected address 0x%08" PRIx32
            "\n",
            locked_address);
        recomp_stop(2, "d3d-texture-lock:0x%08" PRIx32, locked_address);
    }
    *recomp_memory_u32(locked_rect + 4u) = cpu_address;
}

RecompFunction recomp_d3d_texture_lookup_manual(uint32_t guest_address)
{
    switch (guest_address) {
    case D3D_DEVICE_SET_TEXTURE_ADDRESS:
        return recomp_d3d_set_texture_adapter;
    case D3D_TEXTURE_LOCK_RECT_ADDRESS:
        return recomp_d3d_texture_lock_rect_adapter;
    default:
        return NULL;
    }
}
