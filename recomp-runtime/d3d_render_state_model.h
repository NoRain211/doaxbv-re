#ifndef DOAXBV_RECOMP_D3D_RENDER_STATE_MODEL_H
#define DOAXBV_RECOMP_D3D_RENDER_STATE_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/* The subset of guest render state a host rasterizer needs to reproduce the
   guest's depth and alpha-test behaviour. Derived from the raw NV2A methods
   the guest wrote, so a consumer never decodes method numbers itself. */
typedef enum RecompD3dCompareFunc {
    RECOMP_D3D_COMPARE_NEVER = 0,
    RECOMP_D3D_COMPARE_LESS,
    RECOMP_D3D_COMPARE_EQUAL,
    RECOMP_D3D_COMPARE_LESS_EQUAL,
    RECOMP_D3D_COMPARE_GREATER,
    RECOMP_D3D_COMPARE_NOT_EQUAL,
    RECOMP_D3D_COMPARE_GREATER_EQUAL,
    RECOMP_D3D_COMPARE_ALWAYS,
} RecompD3dCompareFunc;

typedef struct RecompD3dDepthState {
    bool depth_test_enable;
    bool depth_write_enable;
    RecompD3dCompareFunc depth_func;
    bool alpha_test_enable;
    RecompD3dCompareFunc alpha_func;
    uint32_t alpha_ref;
} RecompD3dDepthState;

/* Translates one NV2A GL-style comparison enum (0x0200..0x0207) into a
   host-neutral function. Returns false for any other value, leaving the
   output untouched, so an unexpected guest write cannot silently select the
   wrong test. */
bool recomp_d3d_compare_func_from_nv(
    uint32_t value,
    RecompD3dCompareFunc *func);

/* Reads the depth and alpha-test state the guest has established. States the
   guest never wrote fall back to the Xbox D3D8 defaults rather than the
   host's, because the two disagree: D3D8 defaults ZFUNC to LESSEQUAL while
   Direct3D 11 defaults to LESS, which drops every coplanar surface the guest
   expects to win. */
void recomp_d3d_depth_state(
    const RecompD3dRenderStateModel *model,
    RecompD3dDepthState *state);

/* Blend factors, in the order the NV2A GL enums imply. */
typedef enum RecompD3dBlendFactor {
    RECOMP_D3D_BLEND_ZERO,
    RECOMP_D3D_BLEND_ONE,
    RECOMP_D3D_BLEND_SRC_COLOR,
    RECOMP_D3D_BLEND_INV_SRC_COLOR,
    RECOMP_D3D_BLEND_SRC_ALPHA,
    RECOMP_D3D_BLEND_INV_SRC_ALPHA,
    RECOMP_D3D_BLEND_DST_ALPHA,
    RECOMP_D3D_BLEND_INV_DST_ALPHA,
    RECOMP_D3D_BLEND_DST_COLOR,
    RECOMP_D3D_BLEND_INV_DST_COLOR,
    RECOMP_D3D_BLEND_SRC_ALPHA_SATURATE,
} RecompD3dBlendFactor;

typedef enum RecompD3dBlendOp {
    RECOMP_D3D_BLEND_OP_ADD,
    RECOMP_D3D_BLEND_OP_MIN,
    RECOMP_D3D_BLEND_OP_MAX,
    RECOMP_D3D_BLEND_OP_SUBTRACT,
    RECOMP_D3D_BLEND_OP_REVERSE_SUBTRACT,
} RecompD3dBlendOp;

typedef struct RecompD3dBlendState {
    bool blend_enable;
    RecompD3dBlendFactor src_factor;
    RecompD3dBlendFactor dst_factor;
    RecompD3dBlendOp op;
} RecompD3dBlendState;

/* Translates one NV2A blend factor or equation enum. Both return false on an
   unrecognised value, leaving the output untouched. */
bool recomp_d3d_blend_factor_from_nv(
    uint32_t value,
    RecompD3dBlendFactor *factor);
bool recomp_d3d_blend_op_from_nv(uint32_t value, RecompD3dBlendOp *op);

/* Reads the blend state the guest has established. Direct3D 11 defaults
   blending off, so a guest that enables it renders every translucent surface
   opaque until this is applied. */
void recomp_d3d_blend_state(
    const RecompD3dRenderStateModel *model,
    RecompD3dBlendState *state);

#ifdef __cplusplus
}
#endif

#endif
