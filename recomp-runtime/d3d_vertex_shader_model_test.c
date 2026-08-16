#include "d3d_vertex_shader_adapter.h"
#include "d3d_vertex_shader_model.h"
#include "program_manual.h"
#include "runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    TEST_STATIC_BASE = 0x001f0000u,
    TEST_STATIC_SIZE = 0x00007000u,
    TEST_CALL_BASE = 0x2a000000u,
    TEST_CALL_SIZE = 0x00001000u,
    TEST_ENTRY_ESP = TEST_CALL_BASE + 0x200u,
    TEST_DEVICE = 0x001f3120u,
    TEST_DEVICE_GLOBAL = 0x001f2978u,
    TEST_DIRTY_MASK = 0x001f2984u,
    TEST_SHADER_DECLARATION = 0x001f2e00u,
};

static int expect_u32(const char *field, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "D3D vertex shader: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

static void prepare_call(uint8_t *call_memory, uint32_t handle)
{
    uint32_t *stack = (uint32_t *)(void *)(
        call_memory + TEST_ENTRY_ESP - TEST_CALL_BASE);

    memset(call_memory, 0, TEST_CALL_SIZE);
    stack[0] = 0x0010abcdu;
    stack[1] = handle;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xa5a5a5a5u;
}

int recomp_d3d_vertex_shader_model_test(void)
{
    static uint8_t static_memory[TEST_STATIC_SIZE];
    static uint8_t call_memory[TEST_CALL_SIZE];
    const RecompMemoryRegion regions[] = {
        {
            .address = TEST_STATIC_BASE,
            .size = sizeof static_memory,
            .data = static_memory,
        },
        {
            .address = TEST_CALL_BASE,
            .size = sizeof call_memory,
            .data = call_memory,
        },
    };
    RecompD3dVertexShaderModel plain;
    uint32_t declaration[RECOMP_D3D_VERTEX_DECLARATION_WORDS] = {0};
    const RecompD3dVertexShaderModel *model;
    RecompFunction adapter;
    int passed = 1;

    recomp_d3d_vertex_shader_reset(&plain);
    if (!recomp_d3d_build_fixed_function_declaration(0x112u, declaration)) {
        fprintf(stderr, "D3D vertex shader: FVF 0x112 was rejected\n");
        return 0;
    }
    passed &= expect_u32("FVF flags", declaration[0x04u / 4u], 0u);
    passed &= expect_u32("FVF program mask", declaration[0x10u / 4u], 2u);
    passed &= expect_u32("FVF position type", declaration[0x1cu / 4u], 0x32u);
    passed &= expect_u32("FVF normal offset", declaration[0x38u / 4u], 0x0cu);
    passed &= expect_u32("FVF normal type", declaration[0x3cu / 4u], 0x32u);
    passed &= expect_u32("FVF texture offset", declaration[0xa8u / 4u], 0x18u);
    passed &= expect_u32("FVF texture type", declaration[0xacu / 4u], 0x22u);
    if (!recomp_d3d_bind_vertex_shader(
            &plain, 0x112u, RECOMP_D3D_DEFAULT_VERTEX_DECLARATION) ||
        !recomp_d3d_bind_vertex_shader(
            &plain, TEST_SHADER_DECLARATION + 1u, TEST_SHADER_DECLARATION)) {
        fprintf(stderr, "D3D vertex shader: model rejected valid binding\n");
        passed = 0;
    }
    passed &= expect_u32("model handle", plain.handle, TEST_SHADER_DECLARATION + 1u);
    passed &= expect_u32("model declaration", plain.declaration_address, TEST_SHADER_DECLARATION);
    passed &= expect_u32("model count", plain.update_count, 2u);
    if (recomp_d3d_bind_vertex_shader(&plain, 0x112u, TEST_SHADER_DECLARATION)) {
        fprintf(stderr, "D3D vertex shader: model accepted mismatched declaration\n");
        passed = 0;
    }

    memset(static_memory, 0, sizeof static_memory);
    recomp_runtime_init(regions, 2u, NULL, 0u, NULL, 0u);
    *recomp_memory_u32(TEST_DEVICE_GLOBAL) = TEST_DEVICE;
    *recomp_memory_u32(TEST_DEVICE + 0x380u) =
        RECOMP_D3D_DEFAULT_VERTEX_DECLARATION;
    *recomp_memory_u32(TEST_DEVICE + 0x384u) = 2u;
    *recomp_memory_u32(TEST_DIRTY_MASK) = 0x3000u;
    recomp_d3d_vertex_shader_adapter_reset();
    model = recomp_d3d_vertex_shader_adapter_model();

    adapter = recomp_lookup_manual(0x001e7170u);
    if (adapter != recomp_d3d_set_vertex_shader_adapter ||
        recomp_lookup_manual(0x001e716fu) != NULL ||
        recomp_lookup_manual(0x001e7171u) != NULL) {
        fprintf(stderr, "D3D vertex shader: lookup was not exact\n");
        return 0;
    }
    prepare_call(call_memory, 0x112u);
    adapter();
    passed &= expect_u32("adapter handle", model->handle, 0x112u);
    passed &= expect_u32("adapter count", model->update_count, 1u);
    passed &= expect_u32(
        "device declaration",
        *recomp_memory_u32(TEST_DEVICE + 0x380u),
        RECOMP_D3D_DEFAULT_VERTEX_DECLARATION);
    passed &= expect_u32(
        "device handle", *recomp_memory_u32(TEST_DEVICE + 0x384u), 0x112u);
    passed &= expect_u32(
        "device program", *recomp_memory_u32(TEST_DEVICE + 0x388u), 0u);
    passed &= expect_u32(
        "adapter dirty mask", *recomp_memory_u32(TEST_DIRTY_MASK), 0x3470u);
    passed &= expect_u32(
        "adapter declaration mask",
        *recomp_memory_u32(RECOMP_D3D_DEFAULT_VERTEX_DECLARATION + 0x10u),
        2u);
    passed &= expect_u32(
        "adapter ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 8u);
    passed &= expect_u32(
        "void return EAX", recomp_runtime.registers.eax, 0xa5a5a5a5u);

    *recomp_memory_u32(TEST_SHADER_DECLARATION + 4u) = 0x10u;
    *recomp_memory_u32(TEST_SHADER_DECLARATION + 0x10u) = 0x44u;
    prepare_call(call_memory, TEST_SHADER_DECLARATION + 1u);
    adapter();
    passed &= expect_u32(
        "shader declaration",
        *recomp_memory_u32(TEST_DEVICE + 0x380u),
        TEST_SHADER_DECLARATION);
    passed &= expect_u32(
        "shader dirty mask", *recomp_memory_u32(TEST_DIRTY_MASK), 0x3670u);

    recomp_d3d_vertex_shader_adapter_reset();
    passed &= expect_u32("adapter reset count", model->update_count, 0u);
    return passed;
}
