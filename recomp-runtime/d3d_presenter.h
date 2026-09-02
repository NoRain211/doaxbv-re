#ifndef DOAXBV_RECOMP_D3D_PRESENTER_H
#define DOAXBV_RECOMP_D3D_PRESENTER_H

#include <stdbool.h>
#include <stdint.h>

#include "d3d_render_state_model.h"
#include "d3d_texture_model.h"

typedef struct RecompD3dPresenter RecompD3dPresenter;

typedef enum RecompD3dPresenterColorFormat {
    RECOMP_D3D_PRESENTER_COLOR_FORMAT_UNKNOWN,
    /* Xbox linear A8R8G8B8 (0x12) uses BGRA8 host storage. Clear colors
       remain logical RGBA values at the presenter seam. */
    RECOMP_D3D_PRESENTER_COLOR_FORMAT_BGRA8_UNORM,
} RecompD3dPresenterColorFormat;

typedef enum RecompD3dPresenterDepthFormat {
    RECOMP_D3D_PRESENTER_DEPTH_FORMAT_UNKNOWN,
    /* Xbox linear D24S8 (0x2e). */
    RECOMP_D3D_PRESENTER_DEPTH_FORMAT_D24S8,
} RecompD3dPresenterDepthFormat;

typedef struct RecompD3dPresenterConfig {
    uint32_t width;
    uint32_t height;
    RecompD3dPresenterColorFormat color_format;
    RecompD3dPresenterDepthFormat depth_format;
} RecompD3dPresenterConfig;

typedef enum RecompD3dPresenterCommandType {
    RECOMP_D3D_PRESENTER_COMMAND_CLEAR,
    RECOMP_D3D_PRESENTER_COMMAND_PRESENT,
    RECOMP_D3D_PRESENTER_COMMAND_DRAW,
} RecompD3dPresenterCommandType;

typedef struct RecompD3dPresenterClearCommand {
    bool clear_color;
    bool clear_depth;
    bool clear_stencil;
    uint32_t color;
    float z;
    uint32_t stencil;
} RecompD3dPresenterClearCommand;

typedef struct RecompD3dPresenterPresentCommand {
    uint32_t effective_flags;
    uint32_t swap_counter;
} RecompD3dPresenterPresentCommand;

/* One indexed draw. Buffer contents stay in guest memory: the adapter passes
   host pointers and byte counts it has already bounds-checked, so the
   presenter never decodes guest addresses itself. */
typedef struct RecompD3dPresenterDrawCommand {
    uint32_t primitive_type;
    uint32_t index_count;
    uint32_t triangle_count;
    uint32_t vertex_count;
    uint32_t vertex_stride;
    uint32_t fvf;
    const void *vertex_bytes;
    const void *index_bytes;
    /* World-view-projection rows, already composed by the adapter. */
    float transform[16];
    bool has_transform;
    /* Depth and alpha-test state in force for this draw, already decoded by
       the render-state model so the presenter never sees a method number. */
    RecompD3dDepthState depth;
    RecompD3dBlendState blend;
    /* Stage 0 texture for this draw. `texture_bytes` is a host-readable view
       of guest pixel memory, valid only for the duration of the submit. */
    RecompD3dTextureDesc texture;
    bool has_texture;
    const void *texture_bytes;
    uint32_t texture_byte_count;
} RecompD3dPresenterDrawCommand;

typedef struct RecompD3dPresenterCommand {
    RecompD3dPresenterCommandType type;
    union {
        RecompD3dPresenterClearCommand clear;
        RecompD3dPresenterPresentCommand present;
        RecompD3dPresenterDrawCommand draw;
    } data;
} RecompD3dPresenterCommand;

typedef enum RecompD3dPresenterError {
    RECOMP_D3D_PRESENTER_OK,
    RECOMP_D3D_PRESENTER_INVALID_ARGUMENT,
    RECOMP_D3D_PRESENTER_NOT_INITIALIZED,
    RECOMP_D3D_PRESENTER_ALREADY_INITIALIZED,
    RECOMP_D3D_PRESENTER_OUT_OF_MEMORY,
    RECOMP_D3D_PRESENTER_HOST_FAILURE,
    RECOMP_D3D_PRESENTER_WRONG_THREAD,
    RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND,
    RECOMP_D3D_PRESENTER_COMMAND_LIMIT,
} RecompD3dPresenterError;

/* Lifecycle calls and submissions occur on one owning thread. Create requires
   a null output handle, submit consumes the command synchronously, and destroy
   releases all adapter state and nulls the handle. */
#ifdef __cplusplus
extern "C" {
#endif

RecompD3dPresenterError recomp_d3d_presenter_create(
    const RecompD3dPresenterConfig *config,
    RecompD3dPresenter **presenter);
RecompD3dPresenterError recomp_d3d_presenter_submit(
    RecompD3dPresenter *presenter,
    const RecompD3dPresenterCommand *command);
RecompD3dPresenterError recomp_d3d_presenter_destroy(
    RecompD3dPresenter **presenter);

/* Process-wide host pacing toggle. When enabled, presents use
   DXGI_PRESENT_DO_NOT_WAIT instead of vsync so the guest frame loop runs at
   CPU speed. The guest does not depend on wall-clock time (kernel waits are
   already immediate), so this only removes unintended host pacing; it does
   not force guest state. Intended for the full-program runner; the default
   (vsync) remains for windowed tests. */
void recomp_d3d_presenter_set_immediate_present(bool enabled);

/* Observation only: reports which guest texture formats draws actually
   sampled, and which ones a draw asked for but the presenter could not
   upload. Bind counts alone cannot answer that. */
void recomp_d3d_presenter_report_draw_textures(void);

#ifdef __cplusplus
}
#endif

#endif
