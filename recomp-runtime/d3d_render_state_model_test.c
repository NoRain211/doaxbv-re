#include "d3d_render_state_adapter.h"
#include "d3d_render_state_model.h"
#include "program_manual.h"
#include "runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    TEST_STATIC_BASE = 0x001f0000u,
    TEST_STATIC_SIZE = 0x00007000u,
    TEST_CALL_BASE = 0x28000000u,
    TEST_CALL_SIZE = 0x00001000u,
    TEST_ENTRY_ESP = TEST_CALL_BASE + 0x200u,
    TEST_DEVICE = 0x001f3120u,
    TEST_DEVICE_GLOBAL = 0x001f2978u,
    TEST_DIRTY_MASK = 0x001f2984u,
    TEST_EDGE_ANTIALIAS_SHADOW = 0x001f2de4u,
    TEST_SHADOW = 0x001f2dc0u,
    TEST_TEXTURE_FACTOR_SHADOW = 0x001f2dd8u,
    TEST_FILL_MODE_SHADOW = 0x001f2db4u,
    TEST_Z_ENABLE_SHADOW = 0x001f2dc4u,
    TEST_CULL_SHADOW = 0x001f2dd4u,
    TEST_STENCIL_ENABLE_SHADOW = 0x001f2dc8u,
    TEST_MULTISAMPLE_ANTIALIAS_SHADOW = 0x001f2de8u,
    TEST_DIRTY_BIT = 0x00000200u,
};

static int expect_u32(const char *field, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "D3D NormalizeNormals: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

static void prepare_call(
    uint8_t *call_memory,
    uint32_t value,
    uint32_t return_address)
{
    uint32_t *stack = (uint32_t *)(void *)(
        call_memory + TEST_ENTRY_ESP - TEST_CALL_BASE);

    stack[0] = return_address;
    stack[1] = value;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xa5a5a5a5u;
}

int recomp_d3d_render_state_model_test(void)
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
    const RecompD3dRenderStateModel *model;
    RecompFunction adapter;
    int passed = 1;

    /* Plain model: both documented BOOLs are accepted, anything else is
       rejected without touching the model. */
    {
        RecompD3dRenderStateModel plain;
        uint32_t simple_value = 0xa5a5a5a5u;

        recomp_d3d_render_state_reset(&plain);
        passed &= expect_u32("reset value", plain.normalize_normals, 0u);
        passed &= expect_u32(
            "reset update count", plain.normalize_normals_update_count, 0u);

        if (!recomp_d3d_set_normalize_normals(&plain, 0u)) {
            fprintf(stderr, "D3D NormalizeNormals: model rejected 0\n");
            passed = 0;
        }
        passed &= expect_u32("value after 0", plain.normalize_normals, 0u);
        passed &= expect_u32(
            "count after 0", plain.normalize_normals_update_count, 1u);

        if (!recomp_d3d_set_normalize_normals(&plain, 1u)) {
            fprintf(stderr, "D3D NormalizeNormals: model rejected 1\n");
            passed = 0;
        }
        passed &= expect_u32("value after 1", plain.normalize_normals, 1u);
        passed &= expect_u32(
            "count after 1", plain.normalize_normals_update_count, 2u);

        if (recomp_d3d_set_normalize_normals(&plain, 2u)) {
            fprintf(stderr, "D3D NormalizeNormals: model accepted 2\n");
            passed = 0;
        }
        if (recomp_d3d_set_normalize_normals(&plain, 0xffffffffu)) {
            fprintf(
                stderr, "D3D NormalizeNormals: model accepted 0xffffffff\n");
            passed = 0;
        }
        if (recomp_d3d_set_normalize_normals(NULL, 1u)) {
            fprintf(
                stderr, "D3D NormalizeNormals: model accepted null model\n");
            passed = 0;
        }
        passed &= expect_u32(
            "value after rejections", plain.normalize_normals, 1u);
        passed &= expect_u32(
            "count after rejections",
            plain.normalize_normals_update_count,
            2u);
        if (!recomp_d3d_set_texture_factor(&plain, 0u) ||
            !recomp_d3d_set_texture_factor(&plain, 0xffffffffu)) {
            fprintf(stderr, "D3D texture factor: model rejected D3DCOLOR\n");
            passed = 0;
        }
        passed &= expect_u32(
            "texture factor value", plain.texture_factor, 0xffffffffu);
        passed &= expect_u32(
            "texture factor count", plain.texture_factor_update_count, 2u);
        if (recomp_d3d_set_texture_factor(NULL, 0u)) {
            fprintf(stderr, "D3D texture factor: accepted null model\n");
            passed = 0;
        }
        if (!recomp_d3d_set_cull_mode(&plain, 0u) ||
            !recomp_d3d_set_cull_mode(&plain, 0x900u) ||
            !recomp_d3d_set_cull_mode(&plain, 0x901u)) {
            fprintf(stderr, "D3D cull mode: model rejected valid value\n");
            passed = 0;
        }
        passed &= expect_u32("cull value", plain.cull_mode, 0x901u);
        passed &= expect_u32("cull count", plain.cull_mode_update_count, 3u);
        if (recomp_d3d_set_cull_mode(&plain, 1u) ||
            recomp_d3d_set_cull_mode(&plain, 0x902u)) {
            fprintf(stderr, "D3D cull mode: invalid value accepted\n");
            passed = 0;
        }
        passed &= expect_u32(
            "cull count after rejections", plain.cull_mode_update_count, 3u);
        if (!recomp_d3d_set_multisample_antialias(&plain, 0u) ||
            !recomp_d3d_set_multisample_antialias(&plain, 1u)) {
            fprintf(stderr, "D3D multisample AA: model rejected BOOL\n");
            passed = 0;
        }
        passed &= expect_u32(
            "multisample AA value", plain.multisample_antialias, 1u);
        passed &= expect_u32(
            "multisample AA count",
            plain.multisample_antialias_update_count,
            2u);
        if (recomp_d3d_set_multisample_antialias(&plain, 2u)) {
            fprintf(stderr, "D3D multisample AA: model accepted 2\n");
            passed = 0;
        }
        if (!recomp_d3d_set_stencil_enable(&plain, 0u) ||
            !recomp_d3d_set_stencil_enable(&plain, 1u)) {
            fprintf(stderr, "D3D stencil enable: model rejected BOOL\n");
            passed = 0;
        }
        passed &= expect_u32(
            "stencil enable value", plain.stencil_enable, 1u);
        passed &= expect_u32(
            "stencil enable count", plain.stencil_enable_update_count, 2u);
        if (recomp_d3d_set_stencil_enable(&plain, 2u)) {
            fprintf(stderr, "D3D stencil enable: model accepted 2\n");
            passed = 0;
        }
        if (!recomp_d3d_set_z_enable(&plain, 0u) ||
            !recomp_d3d_set_z_enable(&plain, 1u) ||
            !recomp_d3d_set_z_enable(&plain, 2u)) {
            fprintf(stderr, "D3D Z enable: model rejected valid enum\n");
            passed = 0;
        }
        passed &= expect_u32("Z enable value", plain.z_enable, 2u);
        passed &= expect_u32(
            "Z enable count", plain.z_enable_update_count, 3u);
        if (recomp_d3d_set_z_enable(&plain, 3u)) {
            fprintf(stderr, "D3D Z enable: model accepted 3\n");
            passed = 0;
        }
        if (!recomp_d3d_set_fill_mode(&plain, 0x1b00u) ||
            !recomp_d3d_set_fill_mode(&plain, 0x1b01u) ||
            !recomp_d3d_set_fill_mode(&plain, 0x1b02u)) {
            fprintf(stderr, "D3D fill mode: model rejected valid enum\n");
            passed = 0;
        }
        passed &= expect_u32("fill mode value", plain.fill_mode, 0x1b02u);
        passed &= expect_u32(
            "fill mode count", plain.fill_mode_update_count, 3u);
        if (recomp_d3d_set_fill_mode(&plain, 0u)) {
            fprintf(stderr, "D3D fill mode: model accepted 0\n");
            passed = 0;
        }
        if (!recomp_d3d_set_edge_antialias(&plain, 0u) ||
            !recomp_d3d_set_edge_antialias(&plain, 1u)) {
            fprintf(stderr, "D3D edge AA: model rejected BOOL\n");
            passed = 0;
        }
        passed &= expect_u32(
            "edge AA value", plain.edge_antialias, 1u);
        passed &= expect_u32(
            "edge AA count", plain.edge_antialias_update_count, 2u);
        if (recomp_d3d_set_edge_antialias(&plain, 2u)) {
            fprintf(stderr, "D3D edge AA: model accepted 2\n");
            passed = 0;
        }

        if (!recomp_d3d_set_simple_render_state(
                &plain, 0x00040344u, 0x00000302u) ||
            !recomp_d3d_get_simple_render_state(
                &plain, 0x00040344u, &simple_value)) {
            fprintf(stderr, "D3D simple state: model rejected valid method\n");
            passed = 0;
        }
        passed &= expect_u32("simple value", simple_value, 0x00000302u);
        passed &= expect_u32("simple count", plain.simple_update_count, 1u);
        if (!recomp_d3d_set_simple_render_state(
                &plain, 0x00040344u, 0x00000303u) ||
            !recomp_d3d_get_simple_render_state(
                &plain, 0x00040344u, &simple_value)) {
            fprintf(stderr, "D3D simple state: model rejected overwrite\n");
            passed = 0;
        }
        passed &= expect_u32(
            "simple overwritten value", simple_value, 0x00000303u);
        passed &= expect_u32("simple overwrite count", plain.simple_update_count, 2u);
        if (!recomp_d3d_set_simple_render_state(
                &plain, 0x00040260u, 0x11223344u) ||
            !recomp_d3d_get_simple_render_state(
                &plain, 0x00040260u, &simple_value)) {
            fprintf(stderr, "D3D simple state: lower method rejected\n");
            passed = 0;
        }
        passed &= expect_u32(
            "lower method value", simple_value, 0x11223344u);
        if (!recomp_d3d_set_simple_render_state(
                &plain, 0x00040a60u, 0x55667788u) ||
            !recomp_d3d_get_simple_render_state(
                &plain, 0x00040a60u, &simple_value)) {
            fprintf(stderr, "D3D simple state: upper method rejected\n");
            passed = 0;
        }
        passed &= expect_u32(
            "upper method value", simple_value, 0x55667788u);
        passed &= expect_u32(
            "simple observed-domain count", plain.simple_update_count, 4u);
        if (recomp_d3d_get_simple_render_state(
                &plain, 0x00040348u, &simple_value) ||
            recomp_d3d_set_simple_render_state(
                &plain, 0x00040345u, 0u) ||
            recomp_d3d_set_simple_render_state(
                &plain, 0x00030344u, 0u) ||
            recomp_d3d_set_simple_render_state(
                &plain, 0x00042000u, 0u)) {
            fprintf(stderr, "D3D simple state: invalid method accepted\n");
            passed = 0;
        }
        passed &= expect_u32(
            "simple count after rejections", plain.simple_update_count, 4u);

        recomp_d3d_render_state_reset(NULL);
    }

    memset(static_memory, 0xa5, sizeof static_memory);
    memset(call_memory, 0, sizeof call_memory);
    recomp_runtime_init(regions, 2u, NULL, 0u, NULL, 0u);
    *recomp_memory_u32(TEST_DEVICE_GLOBAL) = TEST_DEVICE;
    *recomp_memory_u32(TEST_DIRTY_MASK) = 0x00004100u;
    *recomp_memory_u32(TEST_EDGE_ANTIALIAS_SHADOW) = 0xdeadbeefu;
    *recomp_memory_u32(TEST_SHADOW) = 0xdeadbeefu;
    *recomp_memory_u32(TEST_TEXTURE_FACTOR_SHADOW) = 0xdeadbeefu;
    *recomp_memory_u32(TEST_FILL_MODE_SHADOW) = 0xdeadbeefu;
    *recomp_memory_u32(TEST_Z_ENABLE_SHADOW) = 0xdeadbeefu;
    *recomp_memory_u32(TEST_CULL_SHADOW) = 0xdeadbeefu;
    *recomp_memory_u32(TEST_STENCIL_ENABLE_SHADOW) = 0xdeadbeefu;
    *recomp_memory_u32(TEST_MULTISAMPLE_ANTIALIAS_SHADOW) = 0xdeadbeefu;
    recomp_d3d_render_state_adapter_reset();
    model = recomp_d3d_render_state_adapter_model();

    adapter = recomp_lookup_manual(0x001e5200u);
    if (adapter == NULL) {
        fprintf(stderr, "D3D NormalizeNormals: manual lookup did not resolve\n");
        return 0;
    }
    if (adapter != recomp_d3d_set_normalize_normals_adapter) {
        fprintf(
            stderr, "D3D NormalizeNormals: lookup resolved to another body\n");
        passed = 0;
    }
    if (recomp_lookup_manual(0x001e51fcu) != NULL ||
        recomp_lookup_manual(0x001e5204u) != NULL) {
        fprintf(stderr, "D3D NormalizeNormals: lookup was not exact\n");
        passed = 0;
    }

    adapter = recomp_lookup_manual(0x001e4d80u);
    if (adapter != recomp_d3d_set_simple_render_state_adapter ||
        recomp_lookup_manual(0x001e4d7fu) != NULL ||
        recomp_lookup_manual(0x001e4d81u) != NULL) {
        fprintf(stderr, "D3D simple state: lookup was not exact\n");
        passed = 0;
    } else {
        uint32_t simple_value = 0u;

        *recomp_memory_u32(TEST_ENTRY_ESP) = 0x0010abcdu;
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        recomp_runtime.registers.eax = 0xa5a5a5a5u;
        recomp_runtime.registers.ecx = 0x00040344u;
        recomp_runtime.registers.edx = 0x00000302u;
        adapter();
        if (!recomp_d3d_get_simple_render_state(
                model, 0x00040344u, &simple_value)) {
            fprintf(stderr, "D3D simple state: adapter state missing\n");
            passed = 0;
        }
        passed &= expect_u32(
            "simple adapter value", simple_value, 0x00000302u);
        passed &= expect_u32(
            "simple adapter count", model->simple_update_count, 1u);
        passed &= expect_u32(
            "simple ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 4u);
        passed &= expect_u32(
            "simple ECX", recomp_runtime.registers.ecx, 0x00040344u);
        passed &= expect_u32(
            "simple EDX", recomp_runtime.registers.edx, 0x00000302u);
        passed &= expect_u32(
            "simple dirty unchanged",
            *recomp_memory_u32(TEST_DIRTY_MASK),
            0x00004100u);
        passed &= expect_u32(
            "simple shadow unchanged",
            *recomp_memory_u32(TEST_SHADOW),
            0xdeadbeefu);
    }

    adapter = recomp_lookup_manual(0x001e5240u);
    if (adapter != recomp_d3d_set_texture_factor_adapter ||
        recomp_lookup_manual(0x001e523fu) != NULL ||
        recomp_lookup_manual(0x001e5241u) != NULL) {
        fprintf(stderr, "D3D texture factor: lookup was not exact\n");
        passed = 0;
    } else {
        prepare_call(call_memory, 0xff00aa55u, 0x0010abcdu);
        adapter();
        passed &= expect_u32(
            "texture factor adapter value",
            model->texture_factor,
            0xff00aa55u);
        passed &= expect_u32(
            "texture factor adapter count",
            model->texture_factor_update_count,
            1u);
        passed &= expect_u32(
            "texture factor shadow",
            *recomp_memory_u32(TEST_TEXTURE_FACTOR_SHADOW),
            0xff00aa55u);
        passed &= expect_u32(
            "texture factor ESP",
            recomp_runtime.registers.esp,
            TEST_ENTRY_ESP + 8u);
        passed &= expect_u32(
            "texture factor dirty unchanged",
            *recomp_memory_u32(TEST_DIRTY_MASK),
            0x00004100u);
    }

    adapter = recomp_lookup_manual(0x001e5150u);
    if (adapter != recomp_d3d_set_cull_mode_adapter ||
        recomp_lookup_manual(0x001e514fu) != NULL ||
        recomp_lookup_manual(0x001e5151u) != NULL) {
        fprintf(stderr, "D3D cull mode: lookup was not exact\n");
        passed = 0;
    } else {
        prepare_call(call_memory, 0x901u, 0x0010abcdu);
        adapter();
        passed &= expect_u32("cull adapter value", model->cull_mode, 0x901u);
        passed &= expect_u32(
            "cull adapter count", model->cull_mode_update_count, 1u);
        passed &= expect_u32(
            "cull shadow", *recomp_memory_u32(TEST_CULL_SHADOW), 0x901u);
        passed &= expect_u32(
            "cull ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 8u);
        passed &= expect_u32(
            "cull dirty unchanged",
            *recomp_memory_u32(TEST_DIRTY_MASK),
            0x00004100u);
    }

    adapter = recomp_lookup_manual(0x001e5200u);
    prepare_call(call_memory, 1u, 0x0010abcd);
    adapter();
    passed &= expect_u32("adapter value after 1", model->normalize_normals, 1u);
    passed &= expect_u32(
        "adapter count after 1", model->normalize_normals_update_count, 1u);
    passed &= expect_u32("shadow after 1", *recomp_memory_u32(TEST_SHADOW), 1u);
    passed &= expect_u32(
        "dirty mask after 1",
        *recomp_memory_u32(TEST_DIRTY_MASK),
        0x00004100u | TEST_DIRTY_BIT);
    passed &= expect_u32(
        "shadow following word",
        *recomp_memory_u32(TEST_SHADOW + 4u),
        0xdeadbeefu);
    passed &= expect_u32(
        "shadow preceding word",
        *recomp_memory_u32(TEST_SHADOW - 4u),
        0xa5a5a5a5u);
    passed &= expect_u32(
        "ESP after 1", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 8u);
    passed &= expect_u32(
        "void return EAX", recomp_runtime.registers.eax, 0xa5a5a5a5u);
    passed &= expect_u32(
        "argument preserved", *recomp_memory_u32(TEST_ENTRY_ESP + 4u), 1u);

    prepare_call(call_memory, 0u, 0x0010abcd);
    adapter();
    passed &= expect_u32("adapter value after 0", model->normalize_normals, 0u);
    passed &= expect_u32(
        "adapter count after 0", model->normalize_normals_update_count, 2u);
    passed &= expect_u32("shadow after 0", *recomp_memory_u32(TEST_SHADOW), 0u);
    passed &= expect_u32(
        "dirty mask idempotent",
        *recomp_memory_u32(TEST_DIRTY_MASK),
        0x00004100u | TEST_DIRTY_BIT);
    passed &= expect_u32(
        "ESP after 0", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 8u);

    adapter = recomp_lookup_manual(0x001e6510u);
    if (adapter != recomp_d3d_set_multisample_antialias_adapter ||
        recomp_lookup_manual(0x001e650fu) != NULL ||
        recomp_lookup_manual(0x001e6511u) != NULL) {
        fprintf(stderr, "D3D multisample AA: lookup was not exact\n");
        passed = 0;
    } else {
        prepare_call(call_memory, 0u, 0x0010abcdu);
        adapter();
        passed &= expect_u32(
            "multisample AA adapter value",
            model->multisample_antialias,
            0u);
        passed &= expect_u32(
            "multisample AA adapter count",
            model->multisample_antialias_update_count,
            1u);
        passed &= expect_u32(
            "multisample AA shadow",
            *recomp_memory_u32(TEST_MULTISAMPLE_ANTIALIAS_SHADOW),
            0u);
        passed &= expect_u32(
            "multisample AA ESP",
            recomp_runtime.registers.esp,
            TEST_ENTRY_ESP + 8u);
    }

    adapter = recomp_lookup_manual(0x001e6220u);
    if (adapter != recomp_d3d_set_stencil_enable_adapter ||
        recomp_lookup_manual(0x001e621fu) != NULL ||
        recomp_lookup_manual(0x001e6221u) != NULL) {
        fprintf(stderr, "D3D stencil enable: lookup was not exact\n");
        passed = 0;
    } else {
        prepare_call(call_memory, 0u, 0x0010abcdu);
        adapter();
        passed &= expect_u32(
            "stencil enable adapter value", model->stencil_enable, 0u);
        passed &= expect_u32(
            "stencil enable adapter count",
            model->stencil_enable_update_count,
            1u);
        passed &= expect_u32(
            "stencil enable shadow",
            *recomp_memory_u32(TEST_STENCIL_ENABLE_SHADOW),
            0u);
        passed &= expect_u32(
            "stencil enable ESP",
            recomp_runtime.registers.esp,
            TEST_ENTRY_ESP + 8u);
        passed &= expect_u32(
            "stencil enable dirty unchanged",
            *recomp_memory_u32(TEST_DIRTY_MASK),
            0x00004100u | TEST_DIRTY_BIT);
    }

    adapter = recomp_lookup_manual(0x001e6190u);
    if (adapter != recomp_d3d_set_z_enable_adapter ||
        recomp_lookup_manual(0x001e618fu) != NULL ||
        recomp_lookup_manual(0x001e6191u) != NULL) {
        fprintf(stderr, "D3D Z enable: lookup was not exact\n");
        passed = 0;
    } else {
        prepare_call(call_memory, 1u, 0x0010abcdu);
        adapter();
        passed &= expect_u32("Z enable adapter value", model->z_enable, 1u);
        passed &= expect_u32(
            "Z enable adapter count", model->z_enable_update_count, 1u);
        passed &= expect_u32(
            "Z enable shadow",
            *recomp_memory_u32(TEST_Z_ENABLE_SHADOW),
            1u);
        passed &= expect_u32(
            "Z enable ESP",
            recomp_runtime.registers.esp,
            TEST_ENTRY_ESP + 8u);
    }

    adapter = recomp_lookup_manual(0x001e5470u);
    if (adapter != recomp_d3d_set_fill_mode_adapter ||
        recomp_lookup_manual(0x001e546fu) != NULL ||
        recomp_lookup_manual(0x001e5471u) != NULL) {
        fprintf(stderr, "D3D fill mode: lookup was not exact\n");
        passed = 0;
    } else {
        prepare_call(call_memory, 0x1b02u, 0x0010abcdu);
        adapter();
        passed &= expect_u32(
            "fill mode adapter value", model->fill_mode, 0x1b02u);
        passed &= expect_u32(
            "fill mode adapter count", model->fill_mode_update_count, 1u);
        passed &= expect_u32(
            "fill mode shadow",
            *recomp_memory_u32(TEST_FILL_MODE_SHADOW),
            0x1b02u);
        passed &= expect_u32(
            "fill mode ESP",
            recomp_runtime.registers.esp,
            TEST_ENTRY_ESP + 8u);
    }

    adapter = recomp_lookup_manual(0x001e5080u);
    if (adapter != recomp_d3d_set_edge_antialias_adapter ||
        recomp_lookup_manual(0x001e507fu) != NULL ||
        recomp_lookup_manual(0x001e5081u) != NULL) {
        fprintf(stderr, "D3D edge AA: lookup was not exact\n");
        passed = 0;
    } else {
        prepare_call(call_memory, 0u, 0x0010abcdu);
        adapter();
        passed &= expect_u32(
            "edge AA adapter value", model->edge_antialias, 0u);
        passed &= expect_u32(
            "edge AA adapter count", model->edge_antialias_update_count, 1u);
        passed &= expect_u32(
            "edge AA shadow",
            *recomp_memory_u32(TEST_EDGE_ANTIALIAS_SHADOW),
            0u);
        passed &= expect_u32(
            "edge AA ESP",
            recomp_runtime.registers.esp,
            TEST_ENTRY_ESP + 8u);
    }

    recomp_d3d_render_state_adapter_reset();
    passed &= expect_u32("adapter reset value", model->normalize_normals, 0u);
    passed &= expect_u32(
        "adapter reset count", model->normalize_normals_update_count, 0u);

    return passed;
}
