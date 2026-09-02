#include "d3d_render_state_adapter.h"
#include "stop_report.h"

#include <inttypes.h>
#include <stdio.h>

enum {
    D3D_DEVICE_SET_RENDER_STATE_SIMPLE_ADDRESS = 0x001e4d80u,
    D3D_DEVICE_SET_RENDER_STATE_EDGE_ANTIALIAS_ADDRESS = 0x001e5080u,
    D3D_DEVICE_SET_RENDER_STATE_CULL_MODE_ADDRESS = 0x001e5150u,
    D3D_DEVICE_SET_RENDER_STATE_NORMALIZE_NORMALS_ADDRESS = 0x001e5200u,
    D3D_DEVICE_SET_RENDER_STATE_TEXTURE_FACTOR_ADDRESS = 0x001e5240u,
    D3D_DEVICE_SET_RENDER_STATE_FILL_MODE_ADDRESS = 0x001e5470u,
    D3D_DEVICE_SET_RENDER_STATE_Z_ENABLE_ADDRESS = 0x001e6190u,
    D3D_DEVICE_SET_RENDER_STATE_STENCIL_ENABLE_ADDRESS = 0x001e6220u,
    D3D_DEVICE_SET_RENDER_STATE_MULTISAMPLE_ANTIALIAS_ADDRESS = 0x001e6510u,
    D3D_STATE_DIRTY_MASK = 0x001f2984u,
    D3D_EDGE_ANTIALIAS_SHADOW = 0x001f2de4u,
    D3D_NORMALIZE_NORMALS_SHADOW = 0x001f2dc0u,
    D3D_TEXTURE_FACTOR_SHADOW = 0x001f2dd8u,
    D3D_NORMALIZE_NORMALS_DIRTY_BIT = 0x00000200u,
    D3D_FILL_MODE_SHADOW = 0x001f2db4u,
    D3D_Z_ENABLE_SHADOW = 0x001f2dc4u,
    D3D_CULL_MODE_SHADOW = 0x001f2dd4u,
    D3D_STENCIL_ENABLE_SHADOW = 0x001f2dc8u,
    D3D_MULTISAMPLE_ANTIALIAS_SHADOW = 0x001f2de8u,
    /* stdcall, one DWORD argument: the callee drops the return address and
       the argument from the stack (ret 4). */
    D3D_NORMALIZE_NORMALS_STACK_ADJUST = 8u,
};

static RecompD3dRenderStateModel d3d_render_state_model;

void recomp_d3d_render_state_adapter_reset(void)
{
    recomp_d3d_render_state_reset(&d3d_render_state_model);
}

const RecompD3dRenderStateModel *recomp_d3d_render_state_adapter_model(void)
{
    return &d3d_render_state_model;
}

void recomp_d3d_render_state_adapter_report(void)
{
    const RecompD3dRenderStateModel *model = &d3d_render_state_model;

    fprintf(
        stderr,
        "recomp d3d state: cull=0x%08" PRIx32 "/%" PRIu32
        " zenable=%" PRIu32 "/%" PRIu32
        " fill=0x%08" PRIx32 "/%" PRIu32
        " stencil=%" PRIu32 "/%" PRIu32 "\n",
        model->cull_mode, model->cull_mode_update_count,
        model->z_enable, model->z_enable_update_count,
        model->fill_mode, model->fill_mode_update_count,
        model->stencil_enable, model->stencil_enable_update_count);

    /* Simple states are stored by encoded NV2A method, so the method number
       is the only identity available here; naming them is the consumer's
       job. Reporting the set that is actually present shows which host
       state this title needs. */
    for (uint32_t index = 0u; index < RECOMP_D3D_SIMPLE_METHOD_COUNT;
         ++index) {
        if ((model->simple_present[index / 32u] &
             (1u << (index % 32u))) == 0u) {
            continue;
        }
        fprintf(
            stderr,
            "recomp d3d state: simple method=0x%08" PRIx32
            " value=0x%08" PRIx32 "\n",
            0x00040000u | (index * 4u),
            model->simple_values[index]);
    }
    fprintf(
        stderr,
        "recomp d3d state: simple updates=%" PRIu32 "\n",
        model->simple_update_count);
}

static uint32_t stack_argument(uint32_t entry_esp, uint32_t index)
{
    return *recomp_memory_u32(entry_esp + 4u + index * 4u);
}

/* D3DDevice_SetRenderState_NormalizeNormals. The generated body also queues
   NV2A push-buffer command 0x403A4 through the device global; the adapter
   replaces the call at the API level, so it maintains only the cached-value
   and dirty-flag shadows the rest of the generated D3D8 code reads, and
   never touches the push buffer or the hardware channel. */
void recomp_d3d_set_normalize_normals_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t value = stack_argument(entry_esp, 0u);

    if (!recomp_d3d_set_normalize_normals(&d3d_render_state_model, value)) {
        fprintf(
            stderr,
            "recomp d3d: SetRenderState_NormalizeNormals rejected value"
            " 0x%08" PRIx32 "\n",
            value);
        recomp_stop(2, "d3d-normalize-normals:0x%08" PRIx32, value);
    }

    *recomp_memory_u32(D3D_NORMALIZE_NORMALS_SHADOW) = value;
    *recomp_memory_u32(D3D_STATE_DIRTY_MASK) |=
        D3D_NORMALIZE_NORMALS_DIRTY_BIT;
    recomp_runtime.registers.esp =
        entry_esp + D3D_NORMALIZE_NORMALS_STACK_ADJUST;
}

/* D3DDevice_SetRenderState_TextureFactor. Generated D3D8 repeats the color
   into all sixteen combiner stages. The API replacement retains the raw
   D3DCOLOR and the guest shadow without emitting an NV2A method. */
void recomp_d3d_set_texture_factor_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t value = stack_argument(entry_esp, 0u);

    if (!recomp_d3d_set_texture_factor(&d3d_render_state_model, value)) {
        recomp_stop(2, "d3d-texture-factor:model-unavailable");
    }
    *recomp_memory_u32(D3D_TEXTURE_FACTOR_SHADOW) = value;
    recomp_runtime.registers.esp = entry_esp + 8u;
}

/* D3DDevice_SetRenderState_CullMode. The generated body translates the Xbox
   enum into two hardware methods and caches the original enum. This API-level
   replacement retains the logical state and cache without queuing either
   hardware method. */
void recomp_d3d_set_cull_mode_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t value = stack_argument(entry_esp, 0u);

    if (!recomp_d3d_set_cull_mode(&d3d_render_state_model, value)) {
        fprintf(
            stderr,
            "recomp d3d: SetRenderState_CullMode rejected value"
            " 0x%08" PRIx32 "\n",
            value);
        recomp_stop(2, "d3d-cull-mode:0x%08" PRIx32, value);
    }
    *recomp_memory_u32(D3D_CULL_MODE_SHADOW) = value;
    recomp_runtime.registers.esp = entry_esp + 8u;
}

/* D3DDevice_SetRenderState_MultiSampleAntiAlias. Generated D3D8 combines
   this BOOL with device multisample state before emitting method 0x41D7C.
   The native renderer will consume the logical BOOL; the guest cache remains
   visible to generated state code. */
void recomp_d3d_set_multisample_antialias_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t value = stack_argument(entry_esp, 0u);

    if (!recomp_d3d_set_multisample_antialias(
            &d3d_render_state_model, value)) {
        fprintf(
            stderr,
            "recomp d3d: SetRenderState_MultiSampleAntiAlias rejected"
            " value 0x%08" PRIx32 "\n",
            value);
        recomp_stop(
            2,
            "d3d-multisample-antialias:0x%08" PRIx32,
            value);
    }
    *recomp_memory_u32(D3D_MULTISAMPLE_ANTIALIAS_SHADOW) = value;
    recomp_runtime.registers.esp = entry_esp + 8u;
}

/* D3DDevice_SetRenderState_StencilEnable. Generated D3D8 combines this BOOL
   with the cached stencil function and device capabilities before emitting
   methods 0x41D84 and 0x4032C. This API-level replacement retains only the
   logical state and guest cache consumed by generated state code. */
void recomp_d3d_set_stencil_enable_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t value = stack_argument(entry_esp, 0u);

    if (!recomp_d3d_set_stencil_enable(&d3d_render_state_model, value)) {
        fprintf(
            stderr,
            "recomp d3d: SetRenderState_StencilEnable rejected value"
            " 0x%08" PRIx32 "\n",
            value);
        recomp_stop(2, "d3d-stencil-enable:0x%08" PRIx32, value);
    }
    *recomp_memory_u32(D3D_STENCIL_ENABLE_SHADOW) = value;
    recomp_runtime.registers.esp = entry_esp + 8u;
}

/* D3DDevice_SetRenderState_ZEnable. Generated D3D8 maps the three-value Xbox
   enum to depth-test and W-buffer hardware methods, and performs extra state
   work on transitions to or from W buffering. The API model owns the enum;
   the guest shadow remains available to generated projection/state readers. */
void recomp_d3d_set_z_enable_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t value = stack_argument(entry_esp, 0u);

    if (!recomp_d3d_set_z_enable(&d3d_render_state_model, value)) {
        fprintf(
            stderr,
            "recomp d3d: SetRenderState_ZEnable rejected value"
            " 0x%08" PRIx32 "\n",
            value);
        recomp_stop(2, "d3d-z-enable:0x%08" PRIx32, value);
    }
    *recomp_memory_u32(D3D_Z_ENABLE_SHADOW) = value;
    recomp_runtime.registers.esp = entry_esp + 8u;
}

/* D3DDevice_SetRenderState_FillMode. The Xbox API emits front/back polygon
   modes together; the API model retains the requested enum and the guest
   shadow used by the remaining generated D3D8 state code. */
void recomp_d3d_set_fill_mode_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t value = stack_argument(entry_esp, 0u);

    if (!recomp_d3d_set_fill_mode(&d3d_render_state_model, value)) {
        fprintf(
            stderr,
            "recomp d3d: SetRenderState_FillMode rejected value"
            " 0x%08" PRIx32 "\n",
            value);
        recomp_stop(2, "d3d-fill-mode:0x%08" PRIx32, value);
    }
    *recomp_memory_u32(D3D_FILL_MODE_SHADOW) = value;
    recomp_runtime.registers.esp = entry_esp + 8u;
}

/* D3DDevice_SetRenderState_EdgeAntiAlias. Generated D3D8 writes the same
   BOOL to the front- and back-face hardware fields. The API replacement
   retains the logical BOOL and its guest-visible state shadow. */
void recomp_d3d_set_edge_antialias_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t value = stack_argument(entry_esp, 0u);

    if (!recomp_d3d_set_edge_antialias(&d3d_render_state_model, value)) {
        fprintf(
            stderr,
            "recomp d3d: SetRenderState_EdgeAntiAlias rejected value"
            " 0x%08" PRIx32 "\n",
            value);
        recomp_stop(2, "d3d-edge-antialias:0x%08" PRIx32, value);
    }
    *recomp_memory_u32(D3D_EDGE_ANTIALIAS_SHADOW) = value;
    recomp_runtime.registers.esp = entry_esp + 8u;
}

/* D3DDevice_SetRenderState_Simple is a two-register fastcall: ECX carries
   the encoded one-value push method and EDX carries its value. Generated
   surrounding generated state setters maintain the corresponding D3D state
   shadows. The adapter owns the API state and consumes only the synthetic
   return address, so no push-buffer fill or hardware kickoff can occur. */
void recomp_d3d_set_simple_render_state_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t encoded_method = recomp_runtime.registers.ecx;
    uint32_t value = recomp_runtime.registers.edx;

    if (!recomp_d3d_set_simple_render_state(
            &d3d_render_state_model, encoded_method, value)) {
        fprintf(
            stderr,
            "recomp d3d: SetRenderState_Simple rejected method"
            " 0x%08" PRIx32 "\n",
            encoded_method);
        recomp_stop(
            2,
            "d3d-simple-render-state:0x%08" PRIx32,
            encoded_method);
    }
    recomp_runtime.registers.esp = entry_esp + 4u;
}

RecompFunction recomp_d3d_render_state_lookup_manual(uint32_t guest_address)
{
    switch (guest_address) {
    case D3D_DEVICE_SET_RENDER_STATE_EDGE_ANTIALIAS_ADDRESS:
        return recomp_d3d_set_edge_antialias_adapter;
    case D3D_DEVICE_SET_RENDER_STATE_SIMPLE_ADDRESS:
        return recomp_d3d_set_simple_render_state_adapter;
    case D3D_DEVICE_SET_RENDER_STATE_CULL_MODE_ADDRESS:
        return recomp_d3d_set_cull_mode_adapter;
    case D3D_DEVICE_SET_RENDER_STATE_NORMALIZE_NORMALS_ADDRESS:
        return recomp_d3d_set_normalize_normals_adapter;
    case D3D_DEVICE_SET_RENDER_STATE_TEXTURE_FACTOR_ADDRESS:
        return recomp_d3d_set_texture_factor_adapter;
    case D3D_DEVICE_SET_RENDER_STATE_FILL_MODE_ADDRESS:
        return recomp_d3d_set_fill_mode_adapter;
    case D3D_DEVICE_SET_RENDER_STATE_Z_ENABLE_ADDRESS:
        return recomp_d3d_set_z_enable_adapter;
    case D3D_DEVICE_SET_RENDER_STATE_STENCIL_ENABLE_ADDRESS:
        return recomp_d3d_set_stencil_enable_adapter;
    case D3D_DEVICE_SET_RENDER_STATE_MULTISAMPLE_ANTIALIAS_ADDRESS:
        return recomp_d3d_set_multisample_antialias_adapter;
    default:
        return NULL;
    }
}
