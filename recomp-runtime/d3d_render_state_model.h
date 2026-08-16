#ifndef DOAXBV_RECOMP_D3D_RENDER_STATE_MODEL_H
#define DOAXBV_RECOMP_D3D_RENDER_STATE_MODEL_H

#include <stdbool.h>
#include <stdint.h>

enum {
    /* D3DRS_NORMALIZENORMALS accepts a BOOL; the device-side state is a
       plain DWORD so only the two documented values are representable. */
    RECOMP_D3D_NORMALIZE_NORMALS_OFF = 0u,
    RECOMP_D3D_NORMALIZE_NORMALS_ON = 1u,
    RECOMP_D3D_SIMPLE_METHOD_COUNT = 0x800u,
};

typedef struct RecompD3dRenderStateModel {
    /* Raw Xbox D3DCULL value; host winding translation belongs to a future
       renderer-state consumer. */
    uint32_t cull_mode;
    uint32_t cull_mode_update_count;
    uint32_t normalize_normals;
    uint32_t normalize_normals_update_count;
    uint32_t texture_factor;
    uint32_t texture_factor_update_count;
    uint32_t multisample_antialias;
    uint32_t multisample_antialias_update_count;
    uint32_t stencil_enable;
    uint32_t stencil_enable_update_count;
    uint32_t z_enable;
    uint32_t z_enable_update_count;
    uint32_t fill_mode;
    uint32_t fill_mode_update_count;
    uint32_t edge_antialias;
    uint32_t edge_antialias_update_count;
    uint32_t simple_values[RECOMP_D3D_SIMPLE_METHOD_COUNT];
    uint32_t simple_present[RECOMP_D3D_SIMPLE_METHOD_COUNT / 32u];
    uint32_t simple_update_count;
} RecompD3dRenderStateModel;

void recomp_d3d_render_state_reset(RecompD3dRenderStateModel *model);

/* The value arrives unchecked from the guest. Returns false, leaving the
   model untouched, when it is not one of the two representable BOOLs. */
bool recomp_d3d_set_normalize_normals(
    RecompD3dRenderStateModel *model,
    uint32_t value);
bool recomp_d3d_set_texture_factor(
    RecompD3dRenderStateModel *model,
    uint32_t value);
bool recomp_d3d_set_cull_mode(
    RecompD3dRenderStateModel *model,
    uint32_t value);
bool recomp_d3d_set_multisample_antialias(
    RecompD3dRenderStateModel *model,
    uint32_t value);
bool recomp_d3d_set_stencil_enable(
    RecompD3dRenderStateModel *model,
    uint32_t value);
bool recomp_d3d_set_z_enable(
    RecompD3dRenderStateModel *model,
    uint32_t value);
bool recomp_d3d_set_fill_mode(
    RecompD3dRenderStateModel *model,
    uint32_t value);
bool recomp_d3d_set_edge_antialias(
    RecompD3dRenderStateModel *model,
    uint32_t value);
bool recomp_d3d_set_simple_render_state(
    RecompD3dRenderStateModel *model,
    uint32_t encoded_method,
    uint32_t value);
bool recomp_d3d_get_simple_render_state(
    const RecompD3dRenderStateModel *model,
    uint32_t encoded_method,
    uint32_t *value);

#endif
