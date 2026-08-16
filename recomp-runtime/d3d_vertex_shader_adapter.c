#include "d3d_vertex_shader_adapter.h"
#include "stop_report.h"

#include <inttypes.h>
#include <stdio.h>

enum {
    D3D_DEVICE_SET_VERTEX_SHADER_ADDRESS = 0x001e7170u,
    D3D_DEVICE_GLOBAL = 0x001f2978u,
    D3D_STATE_DIRTY_MASK = 0x001f2984u,
    D3D_VERTEX_DECLARATION_OFFSET = 0x0380u,
    D3D_VERTEX_SHADER_HANDLE_OFFSET = 0x0384u,
    D3D_VERTEX_PROGRAM_OFFSET = 0x0388u,
    D3D_VERTEX_BASE_DIRTY_BITS = 0x00000070u,
    D3D_VERTEX_PROGRAM_DIRTY_BIT = 0x00000400u,
    D3D_VERTEX_DECLARATION_DIRTY_BITS = 0x00001600u,
};

static RecompD3dVertexShaderModel d3d_vertex_shader_model;

void recomp_d3d_vertex_shader_adapter_reset(void)
{
    recomp_d3d_vertex_shader_reset(&d3d_vertex_shader_model);
}

const RecompD3dVertexShaderModel *recomp_d3d_vertex_shader_adapter_model(void)
{
    return &d3d_vertex_shader_model;
}

static uint32_t stack_argument(uint32_t entry_esp, uint32_t index)
{
    return *recomp_memory_u32(entry_esp + 4u + index * 4u);
}

static void fail_vertex_shader(uint32_t handle)
{
    fprintf(
        stderr,
        "recomp d3d: SetVertexShader rejected handle 0x%08" PRIx32 "\n",
        handle);
    recomp_stop(2, "d3d-vertex-shader:0x%08" PRIx32, handle);
}

void recomp_d3d_set_vertex_shader_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t handle = stack_argument(entry_esp, 0u);
    uint32_t device = *recomp_memory_u32(D3D_DEVICE_GLOBAL);
    uint32_t old_declaration;
    uint32_t old_flags;
    uint32_t old_program_mask;
    uint32_t declaration;
    uint32_t new_flags;
    uint32_t new_program_mask;
    uint32_t dirty;

    if (device == 0u) {
        fail_vertex_shader(handle);
    }

    old_declaration =
        *recomp_memory_u32(device + D3D_VERTEX_DECLARATION_OFFSET);
    old_flags = *recomp_memory_u32(old_declaration + 4u);
    old_program_mask = *recomp_memory_u32(old_declaration + 0x10u);

    if ((handle & 1u) == 0u) {
        uint32_t words[RECOMP_D3D_VERTEX_DECLARATION_WORDS];

        declaration = RECOMP_D3D_DEFAULT_VERTEX_DECLARATION;
        for (uint32_t i = 0u;
             i < RECOMP_D3D_VERTEX_DECLARATION_WORDS;
             ++i) {
            words[i] = *recomp_memory_u32(declaration + i * 4u);
        }
        if (!recomp_d3d_build_fixed_function_declaration(handle, words)) {
            fail_vertex_shader(handle);
        }
        for (uint32_t i = 0u;
             i < RECOMP_D3D_VERTEX_DECLARATION_WORDS;
             ++i) {
            *recomp_memory_u32(declaration + i * 4u) = words[i];
        }
    } else {
        declaration = handle - 1u;
    }

    new_flags = *recomp_memory_u32(declaration + 4u);
    new_program_mask = *recomp_memory_u32(declaration + 0x10u);
    dirty = *recomp_memory_u32(D3D_STATE_DIRTY_MASK) |
        D3D_VERTEX_BASE_DIRTY_BITS;
    if (old_program_mask != new_program_mask) {
        dirty |= D3D_VERTEX_PROGRAM_DIRTY_BIT;
    }
    if (old_flags != new_flags) {
        dirty |= D3D_VERTEX_DECLARATION_DIRTY_BITS;
    }

    if (!recomp_d3d_bind_vertex_shader(
            &d3d_vertex_shader_model, handle, declaration)) {
        fail_vertex_shader(handle);
    }
    *recomp_memory_u32(D3D_STATE_DIRTY_MASK) = dirty;
    *recomp_memory_u32(device + D3D_VERTEX_DECLARATION_OFFSET) = declaration;
    *recomp_memory_u32(device + D3D_VERTEX_SHADER_HANDLE_OFFSET) = handle;
    *recomp_memory_u32(device + D3D_VERTEX_PROGRAM_OFFSET) = 0u;
    recomp_runtime.registers.esp = entry_esp + 8u;
}

RecompFunction recomp_d3d_vertex_shader_lookup_manual(uint32_t guest_address)
{
    return guest_address == D3D_DEVICE_SET_VERTEX_SHADER_ADDRESS
        ? recomp_d3d_set_vertex_shader_adapter
        : NULL;
}
