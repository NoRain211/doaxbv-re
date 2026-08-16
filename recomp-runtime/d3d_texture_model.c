#include "d3d_texture_model.h"

#include <stddef.h>

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
