#include "d3d_creation_adapter.h"
#include "d3d_creation_model.h"
#ifdef RECOMP_D3D_FRAME_ENABLED
#include "d3d_frame_adapter.h"
#endif
#include "runtime.h"
#include "stop_report.h"
#include "xbox_memory_layout.h"

#include <inttypes.h>
#include <stdio.h>

enum {
    DIRECT3D_CREATE_DEVICE_ADDRESS = 0x001e9100u,
    D3D_DEVICE_RESET_ADDRESS = 0x001e3b00u,
    D3D_DEVICE_KICK_OFF_ADDRESS = 0x001e9eb0u,
    D3D_MAKE_REQUESTED_SPACE_ADDRESS = 0x001ea190u,
    D3D_DEVICE_GLOBAL = 0x001f2978u,
    D3D_SINGLE_STEP_FLAG = 0x001f297cu,
    D3D_SOFTWARE_CHANNEL_BASE = 0x001f2930u,
    D3D_CREATE_FLAG = 0x001f3620u,
    D3D_PRESENTATION_INTERVAL = 0x001f2d84u,
    D3D_PUSH_BUFFER_LIMIT_DEFAULT = 0x001f5d50u,
    D3D_PUSH_BUFFER_SIZE_DEFAULT = 0x001f5d54u,
};

static RecompD3dCreationModel d3d_creation_model;
static uint32_t d3d_kick_off_calls;

void recomp_d3d_creation_adapter_reset(void)
{
#ifdef RECOMP_D3D_FRAME_ENABLED
    recomp_d3d_frame_adapter_reset();
#endif
    recomp_d3d_creation_reset(&d3d_creation_model);
    d3d_kick_off_calls = 0u;
}

static uint32_t stack_argument(uint32_t entry_esp, uint32_t index)
{
    return *recomp_memory_u32(entry_esp + 4u + index * 4u);
}

static RecompD3dPresentationParameters read_presentation(uint32_t address)
{
    RecompD3dPresentationParameters presentation;
    uint32_t *words = (uint32_t *)(void *)&presentation;

    for (uint32_t i = 0u; i < sizeof presentation / sizeof *words; ++i) {
        words[i] = *recomp_memory_u32(address + i * 4u);
    }
    return presentation;
}

static void write_device_state(const RecompD3dDeviceState *device)
{
    recomp_guest_memset(device->address, 0, RECOMP_D3D_DEVICE_SIZE);
    recomp_guest_memset(device->context, 0, RECOMP_D3D_CONTEXT_SIZE);
    recomp_guest_memset(
        device->push_buffer_base, 0, RECOMP_D3D_PUSH_BUFFER_SIZE);
    *recomp_memory_u32(device->context) = 3u;

    *recomp_memory_u32(device->address + 0x0000u) =
        device->push_buffer_current;
    *recomp_memory_u32(device->address + 0x0004u) =
        device->push_buffer_limit;
    *recomp_memory_u32(device->address + 0x0008u) = device->flags;
    for (uint32_t offset = 0x000cu; offset <= 0x0018u; offset += 4u) {
        *recomp_memory_u32(device->address + offset) = 0x80000000u;
    }
    *recomp_memory_u32(device->address + 0x0024u) =
        device->push_buffer_base;
    *recomp_memory_u32(device->address + 0x0028u) =
        device->push_buffer_end;
    *recomp_memory_u32(device->address + 0x002cu) =
        device->push_buffer_base;
    *recomp_memory_u32(device->address + 0x0030u) = 5u;
    *recomp_memory_u32(device->address + 0x0034u) = device->context;
    *recomp_memory_u32(device->address + 0x003cu) = 0x1fu;
    *recomp_memory_u32(device->address + 0x0048u) = 3u;
    *recomp_memory_u32(device->address + 0x004cu) = 0x00f01710u;

    *recomp_memory_u32(device->address + 0x0380u) = 0x001f2ff8u;
    *recomp_memory_u32(device->address + 0x0384u) = 2u;
    *recomp_memory_u32(device->address + 0x0a98u) = device->width;
    *recomp_memory_u32(device->address + 0x0a9cu) = device->height;
    *recomp_memory_u32(device->address + 0x0aa4u) = 0x3f800000u;
    *recomp_memory_u32(device->address + 0x0aa8u) = 0x3f080000u;
    *recomp_memory_u32(device->address + 0x0aacu) = 0x3f080000u;
    *recomp_memory_u32(device->address + 0x20e0u) = device->flags;
    *recomp_memory_u32(device->address + 0x211cu) = 0x11u;
    *recomp_memory_u32(device->address + 0x2120u) = 1u;
    *recomp_memory_u32(device->address + 0x21b4u) =
        device->buffer_surfaces[0];
    *recomp_memory_u32(device->address + 0x21b8u) =
        device->depth_stencil_surface;
    *recomp_memory_u32(device->address + 0x21bcu) =
        device->back_buffer_surface_count;
    *recomp_memory_u32(device->address + 0x21c0u) =
        device->buffer_surfaces[0];
    *recomp_memory_u32(device->address + 0x21c4u) =
        device->buffer_surfaces[1];
    *recomp_memory_u32(device->address + 0x21c8u) =
        device->buffer_surfaces[2];
    *recomp_memory_u32(device->address + 0x21ccu) =
        device->depth_stencil_surface;
    *recomp_memory_u32(device->address + 0x2328u) = device->width;
    *recomp_memory_u32(device->address + 0x232cu) = device->height;
    *recomp_memory_u32(device->address + 0x23a0u) = 1u;
    *recomp_memory_u32(device->address + 0x23acu) = 0x4b7fffffu;
    *recomp_memory_u32(device->address + 0x23bcu) = device->channel_base;
    *recomp_memory_u32(device->address + 0x23c0u) = device->hardware_base;

    *recomp_memory_u32(device->address + 0x2c20u) = device->context;
    *recomp_memory_u32(device->address + 0x2c24u) = device->context + 0x40u;
    *recomp_memory_u32(device->address + 0x2c28u) = device->context + 0x20u;
}

static void write_reset_device_state(
    const RecompD3dDeviceState *device,
    const RecompD3dPresentationParameters *presentation)
{
    *recomp_memory_u32(device->address + 0x0008u) = device->flags;
    *recomp_memory_u32(device->address + 0x0a98u) = device->width;
    *recomp_memory_u32(device->address + 0x0a9cu) = device->height;
    *recomp_memory_u32(device->address + 0x211cu) =
        presentation->multi_sample_type;
    *recomp_memory_u32(device->address + 0x2120u) =
        presentation->swap_effect;
    *recomp_memory_u32(device->address + 0x21b4u) =
        device->buffer_surfaces[0];
    *recomp_memory_u32(device->address + 0x21b8u) =
        device->depth_stencil_surface;
    *recomp_memory_u32(device->address + 0x21bcu) =
        device->back_buffer_surface_count;
    *recomp_memory_u32(device->address + 0x21c0u) =
        device->buffer_surfaces[0];
    *recomp_memory_u32(device->address + 0x21c4u) =
        device->buffer_surfaces[1];
    *recomp_memory_u32(device->address + 0x21c8u) =
        device->buffer_surfaces[2];
    *recomp_memory_u32(device->address + 0x21ccu) =
        device->depth_stencil_surface;
    *recomp_memory_u32(device->address + 0x2328u) = device->width;
    *recomp_memory_u32(device->address + 0x232cu) = device->height;
    *recomp_memory_u32(D3D_PRESENTATION_INTERVAL) =
        presentation->full_screen_presentation_interval;
}

void recomp_d3d_create_device_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t presentation_address = stack_argument(entry_esp, 4u);
    uint32_t output_address = stack_argument(entry_esp, 5u);
    RecompD3dCreateRequest request = {
        .adapter = stack_argument(entry_esp, 0u),
        .device_type = stack_argument(entry_esp, 1u),
        .focus_window = stack_argument(entry_esp, 2u),
        .behavior_flags = stack_argument(entry_esp, 3u),
    };
    RecompD3dCreateResources resources = {0};
    uint32_t heap_checkpoint = 0u;
    uint32_t output_device = 0u;
    uint32_t result = RECOMP_D3D_INVALID_CALL;

    if (output_address != 0u) {
        *recomp_memory_u32(output_address) = 0u;
    }
    if (presentation_address != 0u && output_address != 0u) {
        request.presentation = read_presentation(presentation_address);
        if (recomp_d3d_create_request_supported(&request)) {
            heap_checkpoint = xbox_HeapCheckpoint();
            resources.context = xbox_HeapAlloc(
                RECOMP_D3D_CONTEXT_SIZE, 4u);
            resources.push_buffer = xbox_HeapAlloc(
                RECOMP_D3D_PUSH_BUFFER_SIZE, 0x1000u);
            result = recomp_d3d_create_device(
                &d3d_creation_model, &request, &resources, &output_device);
        } else {
            const uint32_t *words =
                (const uint32_t *)(const void *)&request.presentation;

            fprintf(
                stderr,
                "recomp d3d: unsupported CreateDevice request"
                " adapter=0x%08" PRIx32 " type=0x%08" PRIx32
                " window=0x%08" PRIx32 " behavior=0x%08" PRIx32 "\n",
                request.adapter,
                request.device_type,
                request.focus_window,
                request.behavior_flags);
            for (uint32_t i = 0u;
                 i < sizeof request.presentation / sizeof *words;
                 ++i) {
                fprintf(
                    stderr,
                    "recomp d3d: presentation[%" PRIu32 "]=0x%08"
                    PRIx32 "\n",
                    i,
                    words[i]);
            }
        }
    }

    if (result == RECOMP_D3D_OK) {
        write_device_state(&d3d_creation_model.device);
#ifdef RECOMP_D3D_FRAME_ENABLED
        recomp_d3d_frame_adapter_initialize(
            &d3d_creation_model.device.presenter_config,
            d3d_creation_model.device.address);
#endif
        if (*recomp_memory_u32(D3D_PUSH_BUFFER_LIMIT_DEFAULT) == 0u) {
            *recomp_memory_u32(D3D_PUSH_BUFFER_LIMIT_DEFAULT) = 0x8000u;
        }
        if (*recomp_memory_u32(D3D_PUSH_BUFFER_SIZE_DEFAULT) == 0u) {
            *recomp_memory_u32(D3D_PUSH_BUFFER_SIZE_DEFAULT) = 0x80000u;
        }
        *recomp_memory_u32(D3D_DEVICE_GLOBAL) = output_device;
        *recomp_memory_u32(D3D_CREATE_FLAG) = 1u;
        *recomp_memory_u32(output_address) = output_device;
    } else {
        if (resources.context != 0u &&
            !xbox_HeapRestore(heap_checkpoint)) {
            fprintf(stderr, "recomp d3d: could not restore guest heap\n");
        }
    }

    fprintf(
        stderr,
        "recomp d3d: Direct3D_CreateDevice result=0x%08" PRIx32
        " device=0x%08" PRIx32 "\n",
        result,
        output_device);
    recomp_runtime.registers.eax = result;
    recomp_runtime.registers.esp = entry_esp + 28u;
}

void recomp_d3d_reset_device_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t presentation_address = stack_argument(entry_esp, 0u);
    RecompD3dPresentationParameters presentation = {0};
    uint32_t result = RECOMP_D3D_INVALID_CALL;

    if (presentation_address != 0u) {
        presentation = read_presentation(presentation_address);
        if (d3d_creation_model.device.created) {
            d3d_creation_model.device.flags = *recomp_memory_u32(
                d3d_creation_model.device.address + 0x0008u);
        }
        result = recomp_d3d_reset_device(
            &d3d_creation_model, &presentation);
    }
    if (result == RECOMP_D3D_OK) {
        write_reset_device_state(
            &d3d_creation_model.device, &presentation);
#ifdef RECOMP_D3D_FRAME_ENABLED
        recomp_d3d_frame_adapter_reset_buffers();
#endif
    } else if (presentation_address != 0u) {
        const uint32_t *words =
            (const uint32_t *)(const void *)&presentation;

        fprintf(stderr, "recomp d3d: unsupported Reset request\n");
        for (uint32_t i = 0u;
             i < sizeof presentation / sizeof *words;
             ++i) {
            fprintf(
                stderr,
                "recomp d3d: reset-presentation[%" PRIu32
                "]=0x%08" PRIx32 "\n",
                i,
                words[i]);
        }
    }

    fprintf(
        stderr,
        "recomp d3d: D3DDevice_Reset result=0x%08" PRIx32 "\n",
        result);
    recomp_runtime.registers.eax = result;
    recomp_runtime.registers.esp = entry_esp + 8u;
}

void recomp_d3d_make_requested_space_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    const RecompD3dDeviceState *device = &d3d_creation_model.device;
    uint32_t push_buffer_current = *recomp_memory_u32(device->address);
    uint32_t requested_bytes = stack_argument(entry_esp, 0u);
    uint32_t reservation_bytes = stack_argument(entry_esp, 1u);
    RecompD3dPushSpace space;

    if (!recomp_d3d_make_push_space(
            &d3d_creation_model,
            push_buffer_current,
            requested_bytes,
            reservation_bytes,
            &space)) {
        fprintf(
            stderr,
            "recomp d3d: MakeRequestedSpace rejected current=0x%08" PRIx32
            " requested=0x%08" PRIx32 " reservation=0x%08" PRIx32 "\n",
            push_buffer_current,
            requested_bytes,
            reservation_bytes);
        recomp_stop(2, "d3d-make-space:model");
    }

    *recomp_memory_u32(push_buffer_current) = space.jump;
    *recomp_memory_u32(device->address + 0x0044u) = space.wrap_offset;
    *recomp_memory_u32(device->address) = space.current;
    *recomp_memory_u32(device->address + 0x0004u) = space.limit;
    recomp_runtime.registers.eax = space.current;
    recomp_runtime.registers.esp = entry_esp + 12u;
}

void recomp_d3d_kick_off_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t device_address = recomp_runtime.registers.ecx;
    uint32_t push_buffer_current;
    uint32_t alternate_command_state;
    uint32_t flags;
    RecompD3dKickOffState state;

    if (device_address != d3d_creation_model.device.address ||
        *recomp_memory_u32(D3D_SINGLE_STEP_FLAG) != 0u) {
        fprintf(
            stderr,
            "recomp d3d: KickOff rejected device=0x%08" PRIx32
            " single_step=0x%08" PRIx32 "\n",
            device_address,
            *recomp_memory_u32(D3D_SINGLE_STEP_FLAG));
        recomp_stop(2, "d3d-kick-off:model");
    }

    push_buffer_current = *recomp_memory_u32(device_address);
    alternate_command_state = *recomp_memory_u32(device_address + 0x035cu);
    flags = *recomp_memory_u32(device_address + 0x0008u);
    if (!recomp_d3d_kick_off(
            &d3d_creation_model,
            push_buffer_current,
            alternate_command_state,
            flags,
            &state)) {
        fprintf(
            stderr,
            "recomp d3d: KickOff rejected current=0x%08" PRIx32
            " alternate=0x%08" PRIx32 " flags=0x%08" PRIx32 "\n",
            push_buffer_current,
            alternate_command_state,
            flags);
        recomp_stop(2, "d3d-kick-off:model");
    }

    *recomp_memory_u32(device_address + 0x002cu) = state.command_state;
    if (state.restore_channel_state) {
        uint32_t context = *recomp_memory_u32(device_address + 0x0034u);

        *recomp_memory_u32(device_address + 0x23bcu) =
            D3D_SOFTWARE_CHANNEL_BASE;
        *recomp_memory_u32(D3D_SOFTWARE_CHANNEL_BASE + 0x0040u) =
            state.dma_put;
        *recomp_memory_u32(D3D_SOFTWARE_CHANNEL_BASE + 0x0044u) =
            state.dma_put;
        *recomp_memory_u32(context) =
            *recomp_memory_u32(device_address + 0x0030u) - 2u;
        *recomp_memory_u32(device_address + 0x257cu) =
            *recomp_memory_u32(device_address + 0x2c10u);
        *recomp_memory_u32(device_address + 0x2bd8u) =
            *recomp_memory_u32(device_address + 0x0040u);
        recomp_runtime.registers.eax = device_address;
        recomp_runtime.registers.edx =
            *recomp_memory_u32(device_address + 0x0040u);
    } else {
        *recomp_memory_u32(device_address + 0x0008u) = state.flags;
        recomp_runtime.registers.eax = 0u;
        recomp_runtime.registers.edx = state.dma_put;
    }
    recomp_runtime.registers.ecx = device_address;
    recomp_runtime.registers.esp = entry_esp + 4u;
    ++d3d_kick_off_calls;
    if (d3d_kick_off_calls == 2u) {
        recomp_stop_at_boundary("d3d-kick-off:second");
    }
}

RecompFunction recomp_d3d_lookup_manual(uint32_t guest_address)
{
    switch (guest_address) {
    case D3D_DEVICE_KICK_OFF_ADDRESS:
        return recomp_d3d_kick_off_adapter;
    case D3D_MAKE_REQUESTED_SPACE_ADDRESS:
        return recomp_d3d_make_requested_space_adapter;
    case D3D_DEVICE_RESET_ADDRESS:
        return recomp_d3d_reset_device_adapter;
    case DIRECT3D_CREATE_DEVICE_ADDRESS:
        return recomp_d3d_create_device_adapter;
    default:
        return NULL;
    }
}
