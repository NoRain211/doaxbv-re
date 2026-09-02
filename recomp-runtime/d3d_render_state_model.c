#include "d3d_render_state_model.h"

enum {
    D3D_SIMPLE_COMMAND_BASE = 0x00040000u,
    D3D_SIMPLE_METHOD_MASK = 0x00001ffcu,
    /* NV2A Kelvin method offsets, as written by the Xbox D3D8 driver. */
    D3D_METHOD_ALPHA_TEST_ENABLE = D3D_SIMPLE_COMMAND_BASE | 0x0300u,
    D3D_METHOD_BLEND_ENABLE = D3D_SIMPLE_COMMAND_BASE | 0x0304u,
    D3D_METHOD_ALPHA_FUNC = D3D_SIMPLE_COMMAND_BASE | 0x033cu,
    D3D_METHOD_ALPHA_REF = D3D_SIMPLE_COMMAND_BASE | 0x0340u,
    D3D_METHOD_BLEND_SRC = D3D_SIMPLE_COMMAND_BASE | 0x0344u,
    D3D_METHOD_BLEND_DST = D3D_SIMPLE_COMMAND_BASE | 0x0348u,
    D3D_METHOD_BLEND_EQUATION = D3D_SIMPLE_COMMAND_BASE | 0x0350u,
    D3D_METHOD_DEPTH_FUNC = D3D_SIMPLE_COMMAND_BASE | 0x0354u,
    D3D_METHOD_DEPTH_MASK = D3D_SIMPLE_COMMAND_BASE | 0x035cu,
    /* Comparisons arrive as GL enums, NEVER through ALWAYS. */
    D3D_NV_COMPARE_BASE = 0x00000200u,
    D3D_NV_COMPARE_LAST = 0x00000207u,
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

bool recomp_d3d_compare_func_from_nv(
    uint32_t value,
    RecompD3dCompareFunc *func)
{
    if (func == NULL || value < D3D_NV_COMPARE_BASE ||
        value > D3D_NV_COMPARE_LAST) {
        return false;
    }
    *func = (RecompD3dCompareFunc)(value - D3D_NV_COMPARE_BASE);
    return true;
}

/* Reads one simple state, or reports absence so the caller can apply the
   guest-side default rather than the host's. */
static bool simple_or_absent(
    const RecompD3dRenderStateModel *model,
    uint32_t method,
    uint32_t *value)
{
    return recomp_d3d_get_simple_render_state(model, method, value);
}

void recomp_d3d_depth_state(
    const RecompD3dRenderStateModel *model,
    RecompD3dDepthState *state)
{
    uint32_t value;

    if (state == NULL) {
        return;
    }

    /* Xbox D3D8 defaults, used whenever the guest has not written the state. */
    state->depth_test_enable = true;
    state->depth_write_enable = true;
    state->depth_func = RECOMP_D3D_COMPARE_LESS_EQUAL;
    state->alpha_test_enable = false;
    state->alpha_func = RECOMP_D3D_COMPARE_ALWAYS;
    state->alpha_ref = 0u;

    if (model == NULL) {
        return;
    }

    /* D3DRS_ZENABLE has its own entry point rather than a simple method, and
       its third value (USEW) still tests depth, so anything non-zero enables
       the test. */
    state->depth_test_enable = model->z_enable != D3D_ZBUFFER_FALSE;

    if (simple_or_absent(model, D3D_METHOD_DEPTH_MASK, &value)) {
        state->depth_write_enable = value != 0u;
    }
    if (simple_or_absent(model, D3D_METHOD_DEPTH_FUNC, &value)) {
        recomp_d3d_compare_func_from_nv(value, &state->depth_func);
    }
    if (simple_or_absent(model, D3D_METHOD_ALPHA_TEST_ENABLE, &value)) {
        state->alpha_test_enable = value != 0u;
    }
    if (simple_or_absent(model, D3D_METHOD_ALPHA_FUNC, &value)) {
        recomp_d3d_compare_func_from_nv(value, &state->alpha_func);
    }
    if (simple_or_absent(model, D3D_METHOD_ALPHA_REF, &value)) {
        state->alpha_ref = value & 0xffu;
    }
}

bool recomp_d3d_blend_factor_from_nv(
    uint32_t value,
    RecompD3dBlendFactor *factor)
{
    if (factor == NULL) {
        return false;
    }
    /* ZERO and ONE sit outside the 0x03xx run, so this cannot be arithmetic. */
    switch (value) {
    case 0x0000u:
        *factor = RECOMP_D3D_BLEND_ZERO;
        return true;
    case 0x0001u:
        *factor = RECOMP_D3D_BLEND_ONE;
        return true;
    case 0x0300u:
        *factor = RECOMP_D3D_BLEND_SRC_COLOR;
        return true;
    case 0x0301u:
        *factor = RECOMP_D3D_BLEND_INV_SRC_COLOR;
        return true;
    case 0x0302u:
        *factor = RECOMP_D3D_BLEND_SRC_ALPHA;
        return true;
    case 0x0303u:
        *factor = RECOMP_D3D_BLEND_INV_SRC_ALPHA;
        return true;
    case 0x0304u:
        *factor = RECOMP_D3D_BLEND_DST_ALPHA;
        return true;
    case 0x0305u:
        *factor = RECOMP_D3D_BLEND_INV_DST_ALPHA;
        return true;
    case 0x0306u:
        *factor = RECOMP_D3D_BLEND_DST_COLOR;
        return true;
    case 0x0307u:
        *factor = RECOMP_D3D_BLEND_INV_DST_COLOR;
        return true;
    case 0x0308u:
        *factor = RECOMP_D3D_BLEND_SRC_ALPHA_SATURATE;
        return true;
    default:
        return false;
    }
}

bool recomp_d3d_blend_op_from_nv(uint32_t value, RecompD3dBlendOp *op)
{
    if (op == NULL) {
        return false;
    }
    switch (value) {
    case 0x8006u:
        *op = RECOMP_D3D_BLEND_OP_ADD;
        return true;
    case 0x8007u:
        *op = RECOMP_D3D_BLEND_OP_MIN;
        return true;
    case 0x8008u:
        *op = RECOMP_D3D_BLEND_OP_MAX;
        return true;
    case 0x800au:
        *op = RECOMP_D3D_BLEND_OP_SUBTRACT;
        return true;
    case 0x800bu:
        *op = RECOMP_D3D_BLEND_OP_REVERSE_SUBTRACT;
        return true;
    default:
        /* The signed NV extensions (0xf005/0xf006) have no host equivalent. */
        return false;
    }
}

void recomp_d3d_blend_state(
    const RecompD3dRenderStateModel *model,
    RecompD3dBlendState *state)
{
    uint32_t value;

    if (state == NULL) {
        return;
    }

    state->blend_enable = false;
    state->src_factor = RECOMP_D3D_BLEND_ONE;
    state->dst_factor = RECOMP_D3D_BLEND_ZERO;
    state->op = RECOMP_D3D_BLEND_OP_ADD;

    if (model == NULL) {
        return;
    }

    if (simple_or_absent(model, D3D_METHOD_BLEND_ENABLE, &value)) {
        state->blend_enable = value != 0u;
    }
    if (simple_or_absent(model, D3D_METHOD_BLEND_SRC, &value)) {
        recomp_d3d_blend_factor_from_nv(value, &state->src_factor);
    }
    if (simple_or_absent(model, D3D_METHOD_BLEND_DST, &value)) {
        recomp_d3d_blend_factor_from_nv(value, &state->dst_factor);
    }
    if (simple_or_absent(model, D3D_METHOD_BLEND_EQUATION, &value)) {
        recomp_d3d_blend_op_from_nv(value, &state->op);
    }
}
