#ifndef DOAXBV_RECOMP_D3D_TEXTURE_MODEL_H
#define DOAXBV_RECOMP_D3D_TEXTURE_MODEL_H

#include <stdbool.h>
#include <stdint.h>

enum {
    RECOMP_D3D_TEXTURE_STAGE_COUNT = 4u,
};

typedef struct RecompD3dTextureModel {
    uint32_t textures[RECOMP_D3D_TEXTURE_STAGE_COUNT];
    uint32_t update_count;
} RecompD3dTextureModel;

void recomp_d3d_texture_reset(RecompD3dTextureModel *model);
bool recomp_d3d_set_texture(
    RecompD3dTextureModel *model,
    uint32_t stage,
    uint32_t texture);
bool recomp_d3d_texture_resolve_cpu_address(
    uint32_t locked_address,
    uint32_t *cpu_address);

#endif
