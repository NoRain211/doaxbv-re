#ifndef DOAXBV_RECOMP_D3D_CREATION_MODEL_H
#define DOAXBV_RECOMP_D3D_CREATION_MODEL_H

#include "d3d_presenter.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    RECOMP_D3D_DEVICE_ADDRESS = 0x001f3120u,
    RECOMP_D3D_DEVICE_SIZE = 0x00002c30u,
    RECOMP_D3D_PUSH_BUFFER_SIZE = 0x00100000u,
    RECOMP_D3D_PUSH_BUFFER_INITIAL_OFFSET = 0x000008e8u,
    RECOMP_D3D_PUSH_BUFFER_LIMIT_OFFSET = 0x00007dfcu,
    RECOMP_D3D_CONTEXT_SIZE = 0x00000060u,
    RECOMP_D3D_OK = 0x00000000u,
    RECOMP_D3D_OUT_OF_MEMORY = 0x8007000eu,
    RECOMP_D3D_INVALID_CALL = 0x8876086cu,
};

typedef struct RecompD3dPresentationParameters {
    uint32_t back_buffer_width;
    uint32_t back_buffer_height;
    uint32_t back_buffer_format;
    uint32_t back_buffer_count;
    uint32_t multi_sample_type;
    uint32_t swap_effect;
    uint32_t device_window;
    uint32_t windowed;
    uint32_t enable_auto_depth_stencil;
    uint32_t auto_depth_stencil_format;
    uint32_t flags;
    uint32_t full_screen_refresh_rate;
    uint32_t full_screen_presentation_interval;
    uint32_t buffer_surfaces[3];
    uint32_t depth_stencil_surface;
} RecompD3dPresentationParameters;

typedef struct RecompD3dCreateRequest {
    uint32_t adapter;
    uint32_t device_type;
    uint32_t focus_window;
    uint32_t behavior_flags;
    RecompD3dPresentationParameters presentation;
} RecompD3dCreateRequest;

typedef struct RecompD3dCreateResources {
    uint32_t context;
    uint32_t push_buffer;
} RecompD3dCreateResources;

typedef struct RecompD3dDeviceState {
    bool created;
    uint32_t address;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t depth_stencil_format;
    RecompD3dPresenterConfig presenter_config;
    uint32_t flags;
    uint32_t context;
    uint32_t push_buffer_current;
    uint32_t push_buffer_limit;
    uint32_t push_buffer_base;
    uint32_t push_buffer_end;
    uint32_t channel_base;
    uint32_t hardware_base;
    uint32_t buffer_surfaces[3];
    uint32_t depth_stencil_surface;
    uint32_t back_buffer_surface_count;
} RecompD3dDeviceState;

typedef struct RecompD3dCreationModel {
    RecompD3dDeviceState device;
} RecompD3dCreationModel;

typedef struct RecompD3dPushSpace {
    uint32_t current;
    uint32_t limit;
    uint32_t wrap_offset;
    uint32_t jump;
} RecompD3dPushSpace;

typedef struct RecompD3dKickOffState {
    bool restore_channel_state;
    uint32_t command_state;
    uint32_t dma_put;
    uint32_t flags;
} RecompD3dKickOffState;

void recomp_d3d_creation_reset(RecompD3dCreationModel *model);
bool recomp_d3d_create_request_supported(
    const RecompD3dCreateRequest *request);
bool recomp_d3d_reset_request_supported(
    const RecompD3dPresentationParameters *presentation);
uint32_t recomp_d3d_create_device(
    RecompD3dCreationModel *model,
    const RecompD3dCreateRequest *request,
    const RecompD3dCreateResources *resources,
    uint32_t *output_device);
uint32_t recomp_d3d_reset_device(
    RecompD3dCreationModel *model,
    const RecompD3dPresentationParameters *presentation);
/* The generated callers enter this seam only after their current reservation
   is exhausted. With no NV2A consumer in the recomp, reuse the completed ring
   immediately instead of exposing or polling the hardware DMA GET register. */
bool recomp_d3d_make_push_space(
    RecompD3dCreationModel *model,
    uint32_t push_buffer_current,
    uint32_t requested_bytes,
    uint32_t reservation_bytes,
    RecompD3dPushSpace *space);
/* Submission is synchronous until a native GPU consumer exists. The model
   preserves the D3D command-selection and shadow-state transition without
   exposing the NV2A kickoff register to generated code. */
bool recomp_d3d_kick_off(
    RecompD3dCreationModel *model,
    uint32_t push_buffer_current,
    uint32_t alternate_command_state,
    uint32_t flags,
    RecompD3dKickOffState *state);

#endif
