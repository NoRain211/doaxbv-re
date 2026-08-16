#include "d3d_frame_model.h"

#include <stdio.h>
#include <string.h>

static int expect_u32(
    const char *field,
    uint32_t actual,
    uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "D3D frame: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

int recomp_d3d_frame_model_test(void)
{
    RecompD3dFrameState state;
    RecompD3dFrameState uninitialized;
    RecompD3dFrameResult result;
    uint32_t clear_z_bits;
    int passed = 1;

    recomp_d3d_frame_reset(&state);
    result = recomp_d3d_frame_clear(
        &state, 0u, 0u, 0xf3u, 0u, 0x3f800000u, 0u);
    passed &= expect_u32(
        "clear before initialization",
        result.error,
        RECOMP_D3D_FRAME_NOT_INITIALIZED);
    result = recomp_d3d_frame_reset_buffers(&state);
    passed &= expect_u32(
        "reset buffers before initialization",
        result.error,
        RECOMP_D3D_FRAME_NOT_INITIALIZED);
    passed &= expect_u32(
        "initialize",
        recomp_d3d_frame_initialize(&state, 720u, 480u),
        RECOMP_D3D_FRAME_OK);
    passed &= expect_u32(
        "repeat initialize",
        recomp_d3d_frame_initialize(&state, 720u, 480u),
        RECOMP_D3D_FRAME_ALREADY_INITIALIZED);

    result = recomp_d3d_frame_clear(
        &state, 0u, 0u, 0xf3u, 0x10203040u, 0x3f800000u, 0u);
    passed &= expect_u32(
        "observed clear", result.error, RECOMP_D3D_FRAME_OK);
    passed &= expect_u32(
        "clear command type",
        result.command.type,
        RECOMP_D3D_PRESENTER_COMMAND_CLEAR);
    passed &= expect_u32(
        "clear color", result.command.data.clear.color, 0x10203040u);
    memcpy(
        &clear_z_bits,
        &result.command.data.clear.z,
        sizeof clear_z_bits);
    passed &= expect_u32(
        "clear z bits", clear_z_bits, 0x3f800000u);
    passed &= expect_u32(
        "clear stencil", result.command.data.clear.stencil, 0u);
    passed &= expect_u32(
        "clear color enabled", result.command.data.clear.clear_color, 1u);
    passed &= expect_u32(
        "clear depth enabled", result.command.data.clear.clear_depth, 1u);
    passed &= expect_u32(
        "clear stencil enabled", result.command.data.clear.clear_stencil, 1u);

    result = recomp_d3d_frame_reset_buffers(&state);
    passed &= expect_u32(
        "reset buffer clear", result.error, RECOMP_D3D_FRAME_OK);
    passed &= expect_u32(
        "reset buffer command type",
        result.command.type,
        RECOMP_D3D_PRESENTER_COMMAND_CLEAR);
    passed &= expect_u32(
        "reset buffer clear color", result.command.data.clear.color, 0u);
    memcpy(
        &clear_z_bits,
        &result.command.data.clear.z,
        sizeof clear_z_bits);
    passed &= expect_u32(
        "reset buffer clear z", clear_z_bits, 0x3f800000u);
    passed &= expect_u32(
        "reset buffer swap counter preserved", state.swap_counter, 0u);

    result = recomp_d3d_frame_clear(
        &state, 1u, 0x12345678u, 0xf3u, 0u, 0x3f800000u, 0u);
    passed &= expect_u32(
        "clear rectangles",
        result.error,
        RECOMP_D3D_FRAME_UNSUPPORTED_CLEAR_RECTS);
    result = recomp_d3d_frame_clear(
        &state, 0u, 0x12345678u, 0xf3u, 0u, 0x3f800000u, 0u);
    passed &= expect_u32(
        "clear rectangle pointer",
        result.error,
        RECOMP_D3D_FRAME_UNSUPPORTED_CLEAR_RECTS);
    result = recomp_d3d_frame_clear(
        &state, 0u, 0u, 0x73u, 0u, 0x3f800000u, 0u);
    passed &= expect_u32(
        "partial color clear",
        result.error,
        RECOMP_D3D_FRAME_UNSUPPORTED_CLEAR_FLAGS);
    result = recomp_d3d_frame_clear(
        &state, 0u, 0u, 0x100u, 0u, 0x3f800000u, 0u);
    passed &= expect_u32(
        "unknown clear flag",
        result.error,
        RECOMP_D3D_FRAME_UNSUPPORTED_CLEAR_FLAGS);
    result = recomp_d3d_frame_clear(
        &state, 0u, 0u, 0xf3u, 0u, 0x7fc00000u, 0u);
    passed &= expect_u32(
        "non-finite clear depth",
        result.error,
        RECOMP_D3D_FRAME_INVALID_CLEAR_DEPTH);
    result = recomp_d3d_frame_clear(
        &state, 0u, 0u, 0xf3u, 0u, 0x3f800000u, 0x100u);
    passed &= expect_u32(
        "out-of-range clear stencil",
        result.error,
        RECOMP_D3D_FRAME_INVALID_CLEAR_STENCIL);
    result = recomp_d3d_frame_clear(
        &state, 0u, 0u, 0xf3u, 0u, 0x40000000u, 0u);
    passed &= expect_u32(
        "out-of-range clear depth",
        result.error,
        RECOMP_D3D_FRAME_INVALID_CLEAR_DEPTH);
    result = recomp_d3d_frame_clear(
        &state, 0u, 0u, 0xf3u, 0u, 0xbf800000u, 0u);
    passed &= expect_u32(
        "negative clear depth",
        result.error,
        RECOMP_D3D_FRAME_INVALID_CLEAR_DEPTH);

    recomp_d3d_frame_reset(&uninitialized);
    result = recomp_d3d_frame_swap(&uninitialized, 0u);
    passed &= expect_u32(
        "swap before initialization",
        result.error,
        RECOMP_D3D_FRAME_NOT_INITIALIZED);
    result = recomp_d3d_frame_swap(&state, 1u);
    passed &= expect_u32(
        "unsupported swap flags",
        result.error,
        RECOMP_D3D_FRAME_UNSUPPORTED_SWAP_FLAGS);
    result = recomp_d3d_frame_swap(&state, 0u);
    passed &= expect_u32(
        "observed swap", result.error, RECOMP_D3D_FRAME_OK);
    passed &= expect_u32(
        "present command type",
        result.command.type,
        RECOMP_D3D_PRESENTER_COMMAND_PRESENT);
    passed &= expect_u32(
        "effective swap flags",
        result.command.data.present.effective_flags,
        RECOMP_D3D_SWAP_DEFAULT_FLAGS);
    passed &= expect_u32(
        "present swap counter",
        result.command.data.present.swap_counter,
        1u);
    passed &= expect_u32("model swap counter", state.swap_counter, 1u);

    return passed;
}
