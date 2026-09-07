#ifndef DOAXBV_RECOMP_D3D_FRAME_MODEL_H
#define DOAXBV_RECOMP_D3D_FRAME_MODEL_H

#include "d3d_presenter.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    RECOMP_D3D_CLEAR_DEPTH = 0x00000001u,
    RECOMP_D3D_CLEAR_STENCIL = 0x00000002u,
    RECOMP_D3D_CLEAR_TARGET_R = 0x00000010u,
    RECOMP_D3D_CLEAR_TARGET_G = 0x00000020u,
    RECOMP_D3D_CLEAR_TARGET_B = 0x00000040u,
    RECOMP_D3D_CLEAR_TARGET_A = 0x00000080u,
    RECOMP_D3D_CLEAR_TARGET =
        RECOMP_D3D_CLEAR_TARGET_R |
        RECOMP_D3D_CLEAR_TARGET_G |
        RECOMP_D3D_CLEAR_TARGET_B |
        RECOMP_D3D_CLEAR_TARGET_A,
    RECOMP_D3D_CLEAR_KNOWN_FLAGS =
        RECOMP_D3D_CLEAR_DEPTH |
        RECOMP_D3D_CLEAR_STENCIL |
        RECOMP_D3D_CLEAR_TARGET,
    RECOMP_D3D_SWAP_DEFAULT_FLAGS = 5u,
};

typedef enum RecompD3dFrameError {
    RECOMP_D3D_FRAME_OK,
    RECOMP_D3D_FRAME_INVALID_ARGUMENT,
    RECOMP_D3D_FRAME_NOT_INITIALIZED,
    RECOMP_D3D_FRAME_ALREADY_INITIALIZED,
    RECOMP_D3D_FRAME_INVALID_SIZE,
    RECOMP_D3D_FRAME_UNSUPPORTED_CLEAR_RECTS,
    RECOMP_D3D_FRAME_UNSUPPORTED_CLEAR_FLAGS,
    RECOMP_D3D_FRAME_INVALID_CLEAR_DEPTH,
    RECOMP_D3D_FRAME_INVALID_CLEAR_STENCIL,
    RECOMP_D3D_FRAME_UNSUPPORTED_SWAP_FLAGS,
    RECOMP_D3D_FRAME_SWAP_COUNTER_OVERFLOW,
} RecompD3dFrameError;

typedef struct RecompD3dFrameState {
    bool initialized;
    uint32_t width;
    uint32_t height;
    uint32_t swap_counter;
} RecompD3dFrameState;

typedef struct RecompD3dFrameResult {
    RecompD3dFrameError error;
    RecompD3dPresenterCommand command;
} RecompD3dFrameResult;

void recomp_d3d_frame_reset(RecompD3dFrameState *state);
RecompD3dFrameError recomp_d3d_frame_initialize(
    RecompD3dFrameState *state,
    uint32_t width,
    uint32_t height);
RecompD3dFrameResult recomp_d3d_frame_clear(
    const RecompD3dFrameState *state,
    uint32_t count,
    uint32_t rects,
    uint32_t flags,
    uint32_t color,
    uint32_t z_bits,
    uint32_t stencil);
RecompD3dFrameResult recomp_d3d_frame_reset_buffers(
    const RecompD3dFrameState *state);
RecompD3dFrameResult recomp_d3d_frame_swap(
    RecompD3dFrameState *state,
    uint32_t flags);

#endif
