#include "d3d_texture_model.h"

#include <stddef.h>

static uint32_t lowest_bit(uint32_t mask)
{
    return mask & (~mask + 1u);
}

/* Distributes the low bits of `value` across the set bits of `mask`, keeping
   their order. This is the scatter half of the Morton interleave. */
static uint32_t spread_bits(uint32_t value, uint32_t mask)
{
    uint32_t result = 0u;
    uint32_t bit = 1u;

    while (mask != 0u) {
        const uint32_t low = lowest_bit(mask);

        if ((value & bit) != 0u) {
            result |= low;
        }
        mask &= ~low;
        bit <<= 1u;
    }
    return result;
}

void recomp_d3d_texture_swizzle_masks(
    uint32_t width,
    uint32_t height,
    uint32_t *mask_x,
    uint32_t *mask_y)
{
    uint32_t x = 0u;
    uint32_t y = 0u;
    uint32_t bit = 1u;
    uint32_t slot = 1u;

    /* Take one bit from each axis in turn. Whichever axis runs out first
       stops contributing, so the taller or wider axis keeps the high bits. */
    while (bit < width || bit < height) {
        if (bit < width) {
            x |= slot;
            slot <<= 1u;
        }
        if (bit < height) {
            y |= slot;
            slot <<= 1u;
        }
        bit <<= 1u;
    }
    if (mask_x != NULL) {
        *mask_x = x;
    }
    if (mask_y != NULL) {
        *mask_y = y;
    }
}

uint32_t recomp_d3d_texture_swizzle_offset(
    uint32_t x,
    uint32_t y,
    uint32_t mask_x,
    uint32_t mask_y)
{
    return spread_bits(x, mask_x) | spread_bits(y, mask_y);
}

static bool is_power_of_two(uint32_t value)
{
    return value != 0u && (value & (value - 1u)) == 0u;
}

bool recomp_d3d_texture_unswizzle(
    const uint8_t *source,
    uint8_t *destination,
    uint32_t width,
    uint32_t height,
    uint32_t bytes_per_texel)
{
    uint32_t mask_x = 0u;
    uint32_t mask_y = 0u;
    uint32_t y;

    if (source == NULL || destination == NULL || bytes_per_texel == 0u ||
        bytes_per_texel > 4u || !is_power_of_two(width) ||
        !is_power_of_two(height)) {
        return false;
    }
    recomp_d3d_texture_swizzle_masks(width, height, &mask_x, &mask_y);

    for (y = 0u; y < height; ++y) {
        const uint32_t row = spread_bits(y, mask_y);
        uint32_t x;

        for (x = 0u; x < width; ++x) {
            const uint32_t from =
                (spread_bits(x, mask_x) | row) * bytes_per_texel;
            const uint32_t to = ((y * width) + x) * bytes_per_texel;
            uint32_t byte;

            for (byte = 0u; byte < bytes_per_texel; ++byte) {
                destination[to + byte] = source[from + byte];
            }
        }
    }
    return true;
}

void recomp_d3d_texture_reset(RecompD3dTextureModel *model)
{
    if (model != NULL) {
        *model = (RecompD3dTextureModel){0};
    }
}

bool recomp_d3d_set_texture(
    RecompD3dTextureModel *model,
    uint32_t stage,
    uint32_t texture)
{
    if (model == NULL || stage >= RECOMP_D3D_TEXTURE_STAGE_COUNT) {
        return false;
    }
    model->textures[stage] = texture;
    ++model->update_count;
    return true;
}

bool recomp_d3d_texture_resolve_cpu_address(
    uint32_t locked_address,
    uint32_t *cpu_address)
{
    enum {
        XBOX_RAM_SIZE = 0x04000000u,
        XBOX_CACHED_ALIAS = 0x80000000u,
    };

    if (cpu_address == NULL || locked_address == 0u) {
        return false;
    }
    if (locked_address < XBOX_RAM_SIZE) {
        *cpu_address = locked_address;
        return true;
    }
    if (locked_address >= XBOX_CACHED_ALIAS &&
        locked_address - XBOX_CACHED_ALIAS < XBOX_RAM_SIZE) {
        *cpu_address = locked_address - XBOX_CACHED_ALIAS;
        return true;
    }
    return false;
}

bool recomp_d3d_texture_describe(
    uint32_t format_dword,
    uint32_t size_dword,
    uint32_t data,
    uint32_t descriptor_byte,
    RecompD3dTextureDesc *out)
{
    RecompD3dTextureDesc desc = {0};

    if (out == NULL) {
        return false;
    }

    desc.format_byte = (format_dword >> 8u) & 0xffu;
    /* The guest's format descriptor packs bits-per-pixel into bits 2-5 and
       flags render-target and depth capability above them. */
    desc.bits_per_pixel = descriptor_byte & 0x3cu;
    desc.render_target = (descriptor_byte & 0x80u) != 0u;
    desc.depth = (descriptor_byte & 0x40u) != 0u;
    /* Xbox guest pointers carry tags in the top nibble. */
    desc.data = data & 0x0fffffffu;

    if (size_dword != 0u) {
        desc.linear = true;
        desc.width = (size_dword & 0xfffu) + 1u;
        desc.height = ((size_dword >> 12u) & 0xfffu) + 1u;
        desc.pitch = ((size_dword >> 24u) + 1u) * 64u;
    } else {
        desc.linear = false;
        desc.width = 1u << ((format_dword >> 20u) & 0xfu);
        desc.height = 1u << ((format_dword >> 24u) & 0xfu);
        desc.pitch = 0u;
    }

    *out = desc;
    return true;
}

static bool texture_desc_equal(
    const RecompD3dTextureDesc *a,
    const RecompD3dTextureDesc *b)
{
    return a->format_byte == b->format_byte &&
        a->linear == b->linear &&
        a->width == b->width &&
        a->height == b->height &&
        a->pitch == b->pitch;
}

void recomp_d3d_texture_census_record(
    RecompD3dTextureCensus *census,
    const RecompD3dTextureDesc *desc,
    uint32_t stage)
{
    uint32_t i;

    if (census == NULL || desc == NULL) {
        return;
    }
    for (i = 0u; i < RECOMP_D3D_TEXTURE_CENSUS_SLOTS; ++i) {
        RecompD3dTextureCensusEntry *entry = &census->entries[i];

        if (!entry->used) {
            entry->used = true;
            entry->desc = *desc;
            entry->stage = stage;
            entry->bind_count = 1u;
            return;
        }
        if (entry->stage == stage && texture_desc_equal(&entry->desc, desc)) {
            ++entry->bind_count;
            return;
        }
    }
    ++census->overflow_count;
}
