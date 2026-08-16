#include "d3d_frame_model.h"

#include <string.h>

void recomp_d3d_frame_reset(RecompD3dFrameState *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof *state);
    }
}

RecompD3dFrameError recomp_d3d_frame_initialize(
    RecompD3dFrameState *state,
    uint32_t width,
    uint32_t height)
{
    if (state == NULL) {
        return RECOMP_D3D_FRAME_INVALID_ARGUMENT;
    }
    if (state->initialized) {
        return RECOMP_D3D_FRAME_ALREADY_INITIALIZED;
    }
    if (width == 0u || height == 0u) {
        return RECOMP_D3D_FRAME_INVALID_SIZE;
    }

    state->initialized = true;
    state->width = width;
    state->height = height;
    state->swap_counter = 0u;
    return RECOMP_D3D_FRAME_OK;
}

RecompD3dFrameResult recomp_d3d_frame_clear(
    const RecompD3dFrameState *state,
    uint32_t count,
    uint32_t rects,
    uint32_t flags,
    uint32_t color,
    uint32_t z_bits,
    uint32_t stencil)
{
    RecompD3dFrameResult result = {0};

    if (state == NULL || !state->initialized) {
        result.error = RECOMP_D3D_FRAME_NOT_INITIALIZED;
        return result;
    }
    if (count != 0u || rects != 0u) {
        result.error = RECOMP_D3D_FRAME_UNSUPPORTED_CLEAR_RECTS;
        return result;
    }
    if (flags != RECOMP_D3D_CLEAR_OBSERVED_FLAGS) {
        result.error = RECOMP_D3D_FRAME_UNSUPPORTED_CLEAR_FLAGS;
        return result;
    }
    if ((z_bits & 0x7f800000u) == 0x7f800000u) {
        result.error = RECOMP_D3D_FRAME_INVALID_CLEAR_DEPTH;
        return result;
    }
    if (stencil > 0xffu) {
        result.error = RECOMP_D3D_FRAME_INVALID_CLEAR_STENCIL;
        return result;
    }

    result.error = RECOMP_D3D_FRAME_OK;
    result.command.type = RECOMP_D3D_PRESENTER_COMMAND_CLEAR;
    result.command.data.clear.clear_color = true;
    result.command.data.clear.clear_depth = true;
    result.command.data.clear.clear_stencil = true;
    result.command.data.clear.color = color;
    memcpy(&result.command.data.clear.z, &z_bits, sizeof z_bits);
    if (result.command.data.clear.z < 0.0f ||
        result.command.data.clear.z > 1.0f) {
        result.error = RECOMP_D3D_FRAME_INVALID_CLEAR_DEPTH;
        return result;
    }
    result.command.data.clear.stencil = stencil;
    return result;
}

RecompD3dFrameResult recomp_d3d_frame_reset_buffers(
    const RecompD3dFrameState *state)
{
    RecompD3dFrameResult result = {0};

    if (state == NULL || !state->initialized) {
        result.error = RECOMP_D3D_FRAME_NOT_INITIALIZED;
        return result;
    }

    result.error = RECOMP_D3D_FRAME_OK;
    result.command.type = RECOMP_D3D_PRESENTER_COMMAND_CLEAR;
    result.command.data.clear.clear_color = true;
    result.command.data.clear.clear_depth = true;
    result.command.data.clear.clear_stencil = true;
    result.command.data.clear.color = 0u;
    result.command.data.clear.z = 1.0f;
    result.command.data.clear.stencil = 0u;
    return result;
}

RecompD3dFrameResult recomp_d3d_frame_swap(
    RecompD3dFrameState *state,
    uint32_t flags)
{
    RecompD3dFrameResult result = {0};

    if (state == NULL || !state->initialized) {
        result.error = RECOMP_D3D_FRAME_NOT_INITIALIZED;
        return result;
    }
    if (flags != 0u) {
        result.error = RECOMP_D3D_FRAME_UNSUPPORTED_SWAP_FLAGS;
        return result;
    }
    if (state->swap_counter == UINT32_MAX) {
        result.error = RECOMP_D3D_FRAME_SWAP_COUNTER_OVERFLOW;
        return result;
    }

    ++state->swap_counter;
    result.error = RECOMP_D3D_FRAME_OK;
    result.command.type = RECOMP_D3D_PRESENTER_COMMAND_PRESENT;
    result.command.data.present.effective_flags =
        RECOMP_D3D_SWAP_DEFAULT_FLAGS;
    result.command.data.present.swap_counter = state->swap_counter;
    return result;
}
