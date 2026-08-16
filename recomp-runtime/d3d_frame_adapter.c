#include "d3d_frame_adapter.h"
#include "d3d_frame_model.h"
#include "d3d_presenter.h"
#include "stop_report.h"

#include <stdio.h>

enum {
    D3D_DEVICE_CLEAR_ADDRESS = 0x001e72d0u,
    D3D_DEVICE_SWAP_ADDRESS = 0x001e8f30u,
};

static RecompD3dFrameState frame_state;
static RecompD3dPresenter *presenter;
static uint32_t frame_device_address;

static uint32_t stack_argument(uint32_t entry_esp, uint32_t index)
{
    return *recomp_memory_u32(entry_esp + 4u + index * 4u);
}

void recomp_d3d_frame_adapter_initialize(
    const RecompD3dPresenterConfig *config,
    uint32_t device_address)
{
    RecompD3dFrameError frame_error;
    RecompD3dPresenterError presenter_error;

    if (config == NULL || device_address == 0u) {
        recomp_stop(2, "d3d-frame-init:device");
    }
    frame_error = recomp_d3d_frame_initialize(
        &frame_state, config->width, config->height);
    if (frame_error != RECOMP_D3D_FRAME_OK) {
        recomp_stop(2, "d3d-frame-init:model:%u", (unsigned)frame_error);
    }
    presenter_error = recomp_d3d_presenter_create(config, &presenter);
    if (presenter_error != RECOMP_D3D_PRESENTER_OK) {
        recomp_d3d_frame_reset(&frame_state);
        recomp_stop(
            2,
            "d3d-frame-init:presenter:%u",
            (unsigned)presenter_error);
    }
    frame_device_address = device_address;
}

void recomp_d3d_frame_adapter_reset(void)
{
    if (presenter != NULL) {
        RecompD3dPresenterError error =
            recomp_d3d_presenter_destroy(&presenter);

        if (error != RECOMP_D3D_PRESENTER_OK) {
            recomp_stop(
                2,
                "d3d-frame-reset:presenter:%u",
                (unsigned)error);
        }
    }
    recomp_d3d_frame_reset(&frame_state);
    frame_device_address = 0u;
}

void recomp_d3d_frame_adapter_reset_buffers(void)
{
    RecompD3dFrameResult result =
        recomp_d3d_frame_reset_buffers(&frame_state);
    RecompD3dPresenterError presenter_error;

    if (result.error != RECOMP_D3D_FRAME_OK) {
        recomp_stop(
            2,
            "d3d-reset:model:%u",
            (unsigned)result.error);
    }
    presenter_error = recomp_d3d_presenter_submit(
        presenter, &result.command);
    if (presenter_error != RECOMP_D3D_PRESENTER_OK) {
        recomp_stop(
            2,
            "d3d-reset:presenter:%u",
            (unsigned)presenter_error);
    }
}

void recomp_d3d_clear_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t saved_eax = recomp_runtime.registers.eax;
    RecompD3dFrameResult result = recomp_d3d_frame_clear(
        &frame_state,
        stack_argument(entry_esp, 0u),
        stack_argument(entry_esp, 1u),
        stack_argument(entry_esp, 2u),
        stack_argument(entry_esp, 3u),
        stack_argument(entry_esp, 4u),
        stack_argument(entry_esp, 5u));
    RecompD3dPresenterError presenter_error;

    if (result.error != RECOMP_D3D_FRAME_OK) {
        fprintf(
            stderr,
            "recomp d3d: Clear model rejected arguments (%u)\n",
            (unsigned)result.error);
        recomp_stop(2, "d3d-clear:model:%u", (unsigned)result.error);
    }
    presenter_error = recomp_d3d_presenter_submit(
        presenter, &result.command);
    if (presenter_error != RECOMP_D3D_PRESENTER_OK) {
        fprintf(
            stderr,
            "recomp d3d: Clear presenter failed (%u)\n",
            (unsigned)presenter_error);
        recomp_stop(
            2,
            "d3d-clear:presenter:%u",
            (unsigned)presenter_error);
    }

    recomp_runtime.registers.eax = saved_eax;
    recomp_runtime.registers.esp = entry_esp + 28u;
}

void recomp_d3d_swap_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    RecompD3dFrameResult result = recomp_d3d_frame_swap(
        &frame_state, stack_argument(entry_esp, 0u));
    RecompD3dPresenterError presenter_error;

    if (result.error != RECOMP_D3D_FRAME_OK) {
        fprintf(
            stderr,
            "recomp d3d: Swap model rejected arguments (%u)\n",
            (unsigned)result.error);
        recomp_stop(2, "d3d-swap:model:%u", (unsigned)result.error);
    }
    presenter_error = recomp_d3d_presenter_submit(
        presenter, &result.command);
    if (presenter_error != RECOMP_D3D_PRESENTER_OK) {
        fprintf(
            stderr,
            "recomp d3d: Swap presenter failed (%u)\n",
            (unsigned)presenter_error);
        recomp_stop(
            2,
            "d3d-swap:presenter:%u",
            (unsigned)presenter_error);
    }

    *recomp_memory_u32(frame_device_address + 0x2c10u) =
        result.command.data.present.swap_counter;
    recomp_runtime.registers.eax =
        result.command.data.present.swap_counter;
    recomp_runtime.registers.esp = entry_esp + 8u;
}

RecompFunction recomp_d3d_frame_lookup_manual(uint32_t guest_address)
{
    switch (guest_address) {
    case D3D_DEVICE_CLEAR_ADDRESS:
        return recomp_d3d_clear_adapter;
    case D3D_DEVICE_SWAP_ADDRESS:
        return recomp_d3d_swap_adapter;
    default:
        return NULL;
    }
}
