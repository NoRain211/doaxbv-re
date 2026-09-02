#include "d3d_draw_model.h"
#include "runtime.h"

#include <stdio.h>

/* The draw adapter delegates to the guest's generated driver body. The unit
   test links the adapter for its lookup entry point only, so this stub
   stands in for that body and simply retires the call. */
void sub_001E78B0(void)
{
    recomp_runtime.registers.esp += 16u;
}

static int expect_u32(const char *field, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "D3D draw: %s was %u, expected %u\n",
        field,
        (unsigned)actual,
        (unsigned)expected);
    return 0;
}

int recomp_d3d_draw_model_test(void)
{
    RecompD3dDrawState state;
    RecompD3dDrawResult result;
    int passed = 1;

    recomp_d3d_draw_reset(&state);
    passed &= expect_u32("reset stride", state.stream0.stride, 0u);

    /* The observed first draw: an 11-triangle fan over 13 indices, with a
       32-byte vertex bound from FVF 0x112 (XYZ | NORMAL | TEX1). */
    state.stream0.vertex_data = 0x822ca8b8u;
    state.stream0.stride = 0x20u;
    state.fvf = 0x112u;

    passed &= expect_u32("fvf 0x112 stride", recomp_d3d_fvf_stride(0x112u), 32u);
    passed &= expect_u32("fvf 0x0112 xyz only", recomp_d3d_fvf_stride(0x002u), 12u);
    passed &= expect_u32(
        "fvf diffuse", recomp_d3d_fvf_stride(0x042u), 16u);
    /* XYZRHW positions are pre-transformed and are not handled by this seam. */
    passed &= expect_u32("fvf xyzrhw rejected", recomp_d3d_fvf_stride(0x004u), 0u);

    /* Both FVFs this title actually draws with. 0x142 carries a diffuse color
       where 0x112 carries a normal, so their texcoords sit at different
       offsets: a consumer using one fixed layout for both reads the wrong
       bytes for whichever it was not built from. */
    {
        RecompD3dVertexLayout layout;

        passed &= expect_u32(
            "layout 0x112 decoded",
            recomp_d3d_fvf_layout(0x112u, &layout) ? 1u : 0u,
            1u);
        passed &= expect_u32("layout 0x112 stride", layout.stride, 32u);
        passed &= expect_u32("layout 0x112 position", layout.position_offset, 0u);
        passed &= expect_u32("layout 0x112 normal", layout.normal_offset, 12u);
        passed &= expect_u32(
            "layout 0x112 diffuse",
            layout.diffuse_offset,
            RECOMP_D3D_FVF_ABSENT);
        passed &= expect_u32("layout 0x112 texcoord", layout.texcoord_offset, 24u);
        passed &= expect_u32("layout 0x112 texcoords", layout.texcoord_count, 1u);

        passed &= expect_u32(
            "layout 0x142 decoded",
            recomp_d3d_fvf_layout(0x142u, &layout) ? 1u : 0u,
            1u);
        passed &= expect_u32("layout 0x142 stride", layout.stride, 24u);
        passed &= expect_u32(
            "layout 0x142 normal",
            layout.normal_offset,
            RECOMP_D3D_FVF_ABSENT);
        passed &= expect_u32("layout 0x142 diffuse", layout.diffuse_offset, 12u);
        passed &= expect_u32("layout 0x142 texcoord", layout.texcoord_offset, 16u);

        /* Stride agrees with the standalone size helper for every FVF. */
        passed &= expect_u32(
            "layout 0x142 stride matches",
            layout.stride,
            recomp_d3d_fvf_stride(0x142u));

        passed &= expect_u32(
            "layout xyzrhw rejected",
            recomp_d3d_fvf_layout(0x004u, &layout) ? 1u : 0u,
            0u);
    }

    passed &= expect_u32(
        "fan triangles",
        recomp_d3d_draw_triangle_count(RECOMP_D3D_PT_TRIANGLEFAN, 13u),
        11u);
    passed &= expect_u32(
        "list triangles",
        recomp_d3d_draw_triangle_count(RECOMP_D3D_PT_TRIANGLELIST, 12u),
        4u);
    passed &= expect_u32(
        "strip triangles",
        recomp_d3d_draw_triangle_count(RECOMP_D3D_PT_TRIANGLESTRIP, 5u),
        3u);
    passed &= expect_u32(
        "degenerate fan",
        recomp_d3d_draw_triangle_count(RECOMP_D3D_PT_TRIANGLEFAN, 2u),
        0u);

    result = recomp_d3d_draw_indexed(
        &state, RECOMP_D3D_PT_TRIANGLEFAN, 13u, 0x822b1fc6u);
    passed &= expect_u32("fan accepted", result.error, RECOMP_D3D_DRAW_OK);
    passed &= expect_u32("plan triangles", result.plan.triangle_count, 11u);
    passed &= expect_u32("plan index bytes", result.plan.index_bytes, 26u);
    passed &= expect_u32("plan stride", result.plan.vertex_stride, 0x20u);
    passed &= expect_u32("plan fvf", result.plan.fvf, 0x112u);

    passed &= expect_u32(
        "vertex span",
        recomp_d3d_draw_vertex_bytes(0x20u, 12u),
        13u * 0x20u);
    passed &= expect_u32(
        "zero stride span", recomp_d3d_draw_vertex_bytes(0u, 12u), 0u);

    result = recomp_d3d_draw_indexed(
        &state, RECOMP_D3D_PT_TRIANGLEFAN, 13u, 0u);
    passed &= expect_u32(
        "null indices rejected",
        result.error,
        RECOMP_D3D_DRAW_INVALID_ARGUMENT);

    result = recomp_d3d_draw_indexed(&state, 1u, 13u, 0x822b1fc6u);
    passed &= expect_u32(
        "point list rejected",
        result.error,
        RECOMP_D3D_DRAW_UNSUPPORTED_PRIMITIVE);

    state.stream0.vertex_data = 0u;
    result = recomp_d3d_draw_indexed(
        &state, RECOMP_D3D_PT_TRIANGLEFAN, 13u, 0x822b1fc6u);
    passed &= expect_u32(
        "unbound stream rejected", result.error, RECOMP_D3D_DRAW_NO_STREAM);

    if (passed) {
        printf("D3D draw model: ok\n");
    }
    return passed;
}
