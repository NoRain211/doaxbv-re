#include "d3d_render_state_model.h"

enum {
    D3D_SIMPLE_COMMAND_BASE = 0x00040000u,
    D3D_SIMPLE_METHOD_MASK = 0x00001ffcu,
    D3D_CULL_NONE = 0u,
    D3D_CULL_CW = 0x00000900u,
    D3D_CULL_CCW = 0x00000901u,
    D3D_ZBUFFER_FALSE = 0u,
    D3D_ZBUFFER_TRUE = 1u,
    D3D_ZBUFFER_USE_W = 2u,
    D3D_FILL_POINT = 0x00001b00u,
    D3D_FILL_WIREFRAME = 0x00001b01u,
    D3D_FILL_SOLID = 0x00001b02u,
};

static bool simple_method_index(
    uint32_t encoded_method,
    uint32_t *index)
{
    if (index == NULL ||
        encoded_method < D3D_SIMPLE_COMMAND_BASE ||
        encoded_method >
            (D3D_SIMPLE_COMMAND_BASE | D3D_SIMPLE_METHOD_MASK) ||
        (encoded_method & 3u) != 0u) {
        return false;
    }
    *index = (encoded_method & D3D_SIMPLE_METHOD_MASK) / 4u;
    return true;
}

void recomp_d3d_render_state_reset(RecompD3dRenderStateModel *model)
{
    if (model != NULL) {
        *model = (RecompD3dRenderStateModel){0};
    }
}

bool recomp_d3d_set_normalize_normals(
    RecompD3dRenderStateModel *model,
    uint32_t value)
{
    if (model == NULL ||
        (value != RECOMP_D3D_NORMALIZE_NORMALS_OFF &&
         value != RECOMP_D3D_NORMALIZE_NORMALS_ON)) {
        return false;
    }
    model->normalize_normals = value;
    ++model->normalize_normals_update_count;
    return true;
}

bool recomp_d3d_set_texture_factor(
    RecompD3dRenderStateModel *model,
    uint32_t value)
{
    if (model == NULL) {
        return false;
    }
    model->texture_factor = value;
    ++model->texture_factor_update_count;
    return true;
}

bool recomp_d3d_set_cull_mode(
    RecompD3dRenderStateModel *model,
    uint32_t value)
{
    if (model == NULL ||
        (value != D3D_CULL_NONE && value != D3D_CULL_CW &&
         value != D3D_CULL_CCW)) {
        return false;
    }
    model->cull_mode = value;
    ++model->cull_mode_update_count;
    return true;
}

bool recomp_d3d_set_multisample_antialias(
    RecompD3dRenderStateModel *model,
    uint32_t value)
{
    if (model == NULL || (value != 0u && value != 1u)) {
        return false;
    }
    model->multisample_antialias = value;
    ++model->multisample_antialias_update_count;
    return true;
}

bool recomp_d3d_set_stencil_enable(
    RecompD3dRenderStateModel *model,
    uint32_t value)
{
    if (model == NULL || (value != 0u && value != 1u)) {
        return false;
    }
    model->stencil_enable = value;
    ++model->stencil_enable_update_count;
    return true;
}

bool recomp_d3d_set_z_enable(
    RecompD3dRenderStateModel *model,
    uint32_t value)
{
    if (model == NULL ||
        (value != D3D_ZBUFFER_FALSE && value != D3D_ZBUFFER_TRUE &&
         value != D3D_ZBUFFER_USE_W)) {
        return false;
    }
    model->z_enable = value;
    ++model->z_enable_update_count;
    return true;
}

bool recomp_d3d_set_fill_mode(
    RecompD3dRenderStateModel *model,
    uint32_t value)
{
    if (model == NULL ||
        (value != D3D_FILL_POINT && value != D3D_FILL_WIREFRAME &&
         value != D3D_FILL_SOLID)) {
        return false;
    }
    model->fill_mode = value;
    ++model->fill_mode_update_count;
    return true;
}

bool recomp_d3d_set_edge_antialias(
    RecompD3dRenderStateModel *model,
    uint32_t value)
{
    if (model == NULL || (value != 0u && value != 1u)) {
        return false;
    }
    model->edge_antialias = value;
    ++model->edge_antialias_update_count;
    return true;
}

bool recomp_d3d_set_simple_render_state(
    RecompD3dRenderStateModel *model,
    uint32_t encoded_method,
    uint32_t value)
{
    uint32_t index;

    if (model == NULL || !simple_method_index(encoded_method, &index)) {
        return false;
    }
    model->simple_values[index] = value;
    model->simple_present[index / 32u] |= 1u << (index % 32u);
    ++model->simple_update_count;
    return true;
}

bool recomp_d3d_get_simple_render_state(
    const RecompD3dRenderStateModel *model,
    uint32_t encoded_method,
    uint32_t *value)
{
    uint32_t index;

    if (model == NULL || value == NULL ||
        !simple_method_index(encoded_method, &index) ||
        (model->simple_present[index / 32u] &
         (1u << (index % 32u))) == 0u) {
        return false;
    }
    *value = model->simple_values[index];
    return true;
}
