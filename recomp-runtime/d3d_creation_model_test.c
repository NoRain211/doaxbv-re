#include "d3d_creation_adapter.h"
#include "d3d_creation_model.h"
#include "d3d_presenter_memory_test.h"
#include "program_manual.h"
#include "runtime.h"
#include "xbox_memory_layout.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    TEST_STATIC_BASE = 0x001f0000u,
    TEST_STATIC_SIZE = 0x00007000u,
    TEST_CALL_BASE = 0x26000000u,
    TEST_CALL_SIZE = 0x00001000u,
    TEST_HEAP_BASE = 0x27000000u,
    TEST_HEAP_SIZE = 0x00200000u,
    TEST_PRESENTATION = TEST_CALL_BASE + 0x100u,
    TEST_OUTPUT = TEST_CALL_BASE + 0x200u,
    TEST_ENTRY_ESP = TEST_CALL_BASE + 0x300u,
};

void recomp_test_heap_reset(uint32_t cursor, int fail_after);

void sub_0018322D(void)
{
}

static int expect_u32(const char *field, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "D3D creation: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

static RecompD3dPresentationParameters observed_presentation(void)
{
    return (RecompD3dPresentationParameters){
        .back_buffer_width = 0x000002d0u,
        .back_buffer_height = 0x000001e0u,
        .back_buffer_format = 0x00000012u,
        .back_buffer_count = 1u,
        .multi_sample_type = 0x00000011u,
        .swap_effect = 1u,
        .device_window = 0u,
        .windowed = 0u,
        .enable_auto_depth_stencil = 1u,
        .auto_depth_stencil_format = 0x0000002eu,
        .flags = 0u,
        .full_screen_refresh_rate = 0u,
        .full_screen_presentation_interval = 0x80000001u,
        .buffer_surfaces = {0x00a23ab0u, 0x00a23ac8u, 0u},
        .depth_stencil_surface = 0x00a23ae0u,
    };
}

static void prepare_call(
    uint8_t *call_memory,
    RecompD3dPresentationParameters presentation,
    uint32_t behavior_flags)
{
    uint32_t *stack = (uint32_t *)(void *)(
        call_memory + TEST_ENTRY_ESP - TEST_CALL_BASE);

    memset(call_memory, 0, TEST_CALL_SIZE);
    memcpy(
        call_memory + TEST_PRESENTATION - TEST_CALL_BASE,
        &presentation,
        sizeof presentation);
    *recomp_memory_u32(TEST_OUTPUT) = 0xa5a5a5a5u;
    stack[0] = 0u;
    stack[1] = 0u;
    stack[2] = 1u;
    stack[3] = 0u;
    stack[4] = behavior_flags;
    stack[5] = TEST_PRESENTATION;
    stack[6] = TEST_OUTPUT;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
}

static void prepare_reset_call(
    uint8_t *call_memory,
    RecompD3dPresentationParameters presentation)
{
    uint32_t *stack = (uint32_t *)(void *)(
        call_memory + TEST_ENTRY_ESP - TEST_CALL_BASE);

    memset(call_memory, 0, TEST_CALL_SIZE);
    memcpy(
        call_memory + TEST_PRESENTATION - TEST_CALL_BASE,
        &presentation,
        sizeof presentation);
    stack[0] = 0u;
    stack[1] = TEST_PRESENTATION;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
}

static void prepare_make_space_call(
    uint8_t *call_memory,
    uint32_t requested_bytes,
    uint32_t reservation_bytes)
{
    uint32_t *stack = (uint32_t *)(void *)(
        call_memory + TEST_ENTRY_ESP - TEST_CALL_BASE);

    stack[0] = 0x0010abcdu;
    stack[1] = requested_bytes;
    stack[2] = reservation_bytes;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
}

int recomp_d3d_creation_model_test(void)
{
    static uint8_t static_memory[TEST_STATIC_SIZE];
    static uint8_t call_memory[TEST_CALL_SIZE];
    static uint8_t heap_memory[TEST_HEAP_SIZE];
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
        {
            .address = TEST_HEAP_BASE,
            .size = sizeof heap_memory,
            .data = heap_memory,
        },
    };
    RecompD3dPresentationParameters presentation = observed_presentation();
    RecompD3dCreateRequest xemu_request = {
        .adapter = 0u,
        .device_type = 1u,
        .focus_window = 0u,
        .behavior_flags = 0x40u,
        .presentation = presentation,
    };
    RecompD3dPresenterMemorySnapshot frame_snapshot;
    RecompFunction adapter;
    RecompFunction reset_adapter;
    int passed = 1;

    if (!recomp_d3d_create_request_supported(&xemu_request)) {
        fprintf(stderr, "D3D creation: xemu request was rejected\n");
        passed = 0;
    }
    {
        /* The plain model must derive the contiguous nonzero back-buffer
           count from the presentation: {A23AB0, A23AC8, 0} -> 2. */
        RecompD3dCreationModel model;
        const RecompD3dCreateResources resources = {1u, 2u};
        RecompD3dPresentationParameters reset_presentation = presentation;
        uint32_t output_device = 0u;

        recomp_d3d_creation_reset(&model);
        passed &= expect_u32(
            "model create result",
            recomp_d3d_create_device(
                &model, &xemu_request, &resources, &output_device),
            RECOMP_D3D_OK);
        passed &= expect_u32(
            "model back-buffer surface count",
            model.device.back_buffer_surface_count,
            2u);
        passed &= expect_u32(
            "model buffer surface 0",
            model.device.buffer_surfaces[0],
            0x00a23ab0u);
        passed &= expect_u32(
            "model buffer surface 1",
            model.device.buffer_surfaces[1],
            0x00a23ac8u);
        passed &= expect_u32(
            "model buffer surface 2",
            model.device.buffer_surfaces[2],
            0u);
        passed &= expect_u32(
            "model depth-stencil surface",
            model.device.depth_stencil_surface,
            0x00a23ae0u);
        passed &= expect_u32(
            "model depth-stencil format",
            model.device.depth_stencil_format,
            0x0000002eu);
        {
            RecompD3dPushSpace space;

            passed &= expect_u32(
                "model make-space result",
                recomp_d3d_make_push_space(
                    &model, 0x00007ff0u, 0x4000u, 0x8000u, &space),
                1u);
            passed &= expect_u32(
                "model wrapped current", space.current, 2u);
            passed &= expect_u32(
                "model wrapped limit", space.limit, 0x00007dfeu);
            passed &= expect_u32(
                "model wrap offset", space.wrap_offset, 0x00007feeu);
            passed &= expect_u32(
                "model jump", space.jump, 3u);
            passed &= expect_u32(
                "model rejects oversized reservation",
                recomp_d3d_make_push_space(
                    &model,
                    0x00007ff0u,
                    0x4000u,
                    RECOMP_D3D_PUSH_BUFFER_SIZE + 1u,
                    &space),
                0u);
        }
        {
            RecompD3dKickOffState kick_off;

            passed &= expect_u32(
                "model kickoff result",
                recomp_d3d_kick_off(
                    &model, 0x00007ff0u, 0u, 3u, &kick_off),
                1u);
            passed &= expect_u32(
                "model kickoff command", kick_off.command_state, 0x00007ff0u);
            passed &= expect_u32(
                "model kickoff DMA PUT", kick_off.dma_put, 0x00007ff0u);
            passed &= expect_u32(
                "model kickoff flags", kick_off.flags, 0x00002003u);
            passed &= expect_u32(
                "model kickoff first submit",
                kick_off.restore_channel_state,
                0u);
            passed &= expect_u32(
                "model repeated kickoff result",
                recomp_d3d_kick_off(
                    &model,
                    0x00007ff0u,
                    0x000080f0u,
                    0x00002007u,
                    &kick_off),
                1u);
            passed &= expect_u32(
                "model repeated kickoff command",
                kick_off.command_state,
                0x000080f0u);
            passed &= expect_u32(
                "model repeated kickoff DMA PUT",
                kick_off.dma_put,
                0x00007ff0u);
            passed &= expect_u32(
                "model repeated kickoff restores channel",
                kick_off.restore_channel_state,
                1u);
        }
        passed &= expect_u32(
            "model presenter color format",
            model.device.presenter_config.color_format,
            RECOMP_D3D_PRESENTER_COLOR_FORMAT_BGRA8_UNORM);
        passed &= expect_u32(
            "model presenter depth format",
            model.device.presenter_config.depth_format,
            RECOMP_D3D_PRESENTER_DEPTH_FORMAT_D24S8);
        reset_presentation.flags = 0x10u;
        reset_presentation.full_screen_presentation_interval = 1u;
        model.device.flags = 0x00004001u;
        passed &= expect_u32(
            "model reset result",
            recomp_d3d_reset_device(&model, &reset_presentation),
            RECOMP_D3D_OK);
        passed &= expect_u32(
            "model reset device flags", model.device.flags, 1u);
        passed &= expect_u32(
            "model reset surface count",
            model.device.back_buffer_surface_count,
            2u);
        reset_presentation.full_screen_presentation_interval = 0x80000001u;
        passed &= expect_u32(
            "model second observed reset interval",
            recomp_d3d_reset_device(&model, &reset_presentation),
            RECOMP_D3D_OK);
        reset_presentation.full_screen_presentation_interval = 2u;
        passed &= expect_u32(
            "model unsupported reset interval",
            recomp_d3d_reset_device(&model, &reset_presentation),
            RECOMP_D3D_INVALID_CALL);
    }
    presentation.flags = 0x10u;
    memset(static_memory, 0xa5, sizeof static_memory);
    memset(heap_memory, 0xa5, sizeof heap_memory);
    recomp_runtime_init(regions, 3u, NULL, 0u, NULL, 0u);
    recomp_test_heap_reset(TEST_HEAP_BASE, -1);
    prepare_call(call_memory, presentation, 0x40u);

    adapter = recomp_lookup_manual(0x001e9100u);
    if (adapter == NULL) {
        fprintf(stderr, "D3D creation: manual lookup did not resolve\n");
        return 0;
    }
    adapter();

    passed &= expect_u32(
        "success HRESULT", recomp_runtime.registers.eax, RECOMP_D3D_OK);
    passed &= expect_u32(
        "success ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 28u);
    passed &= expect_u32(
        "success output",
        *recomp_memory_u32(TEST_OUTPUT),
        RECOMP_D3D_DEVICE_ADDRESS);
    passed &= expect_u32(
        "device global",
        *recomp_memory_u32(0x001f2978u),
        RECOMP_D3D_DEVICE_ADDRESS);
    passed &= expect_u32(
        "creation flag", *recomp_memory_u32(0x001f3620u), 1u);
    passed &= expect_u32(
        "creation device flags",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 8u),
        3u);

    {
        RecompFunction make_space_adapter =
            recomp_lookup_manual(0x001ea190u);
        uint32_t push_buffer = *recomp_memory_u32(
            RECOMP_D3D_DEVICE_ADDRESS + 0x0024u);
        uint32_t old_current = push_buffer + 0x7df0u;

        if (make_space_adapter == NULL) {
            fprintf(stderr, "D3D creation: MakeRequestedSpace lookup failed\n");
            return 0;
        }
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS) = old_current;
        prepare_make_space_call(call_memory, 0x4000u, 0x8000u);
        make_space_adapter();
        passed &= expect_u32(
            "make-space ESP",
            recomp_runtime.registers.esp,
            TEST_ENTRY_ESP + 12u);
        passed &= expect_u32(
            "make-space result", recomp_runtime.registers.eax, push_buffer);
        passed &= expect_u32(
            "make-space current",
            *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS),
            push_buffer);
        passed &= expect_u32(
            "make-space limit",
            *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 4u),
            push_buffer + RECOMP_D3D_PUSH_BUFFER_LIMIT_OFFSET);
        passed &= expect_u32(
            "make-space wrap offset",
            *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x44u),
            0x7df0u);
        passed &= expect_u32(
            "make-space jump",
            *recomp_memory_u32(old_current),
            (push_buffer & 0x0fffffffu) + 1u);

        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS) =
            push_buffer + RECOMP_D3D_PUSH_BUFFER_INITIAL_OFFSET;
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 4u) =
            push_buffer + RECOMP_D3D_PUSH_BUFFER_LIMIT_OFFSET;
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x44u) = 0u;
    }

    {
        RecompFunction kick_off_adapter =
            recomp_lookup_manual(0x001e9eb0u);
        uint32_t push_buffer_current = *recomp_memory_u32(
            RECOMP_D3D_DEVICE_ADDRESS);
        uint32_t context = *recomp_memory_u32(
            RECOMP_D3D_DEVICE_ADDRESS + 0x0034u);

        if (kick_off_adapter == NULL) {
            fprintf(stderr, "D3D creation: KickOff lookup failed\n");
            return 0;
        }
        *recomp_memory_u32(0x001f297cu) = 0u;
        recomp_runtime.registers.ecx = RECOMP_D3D_DEVICE_ADDRESS;
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        kick_off_adapter();
        passed &= expect_u32(
            "kickoff ESP",
            recomp_runtime.registers.esp,
            TEST_ENTRY_ESP + 4u);
        passed &= expect_u32(
            "kickoff command state",
            *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x002cu),
            push_buffer_current);
        passed &= expect_u32(
            "kickoff flags",
            *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x0008u),
            0x00002003u);
        passed &= expect_u32(
            "kickoff DMA PUT register",
            recomp_runtime.registers.edx,
            push_buffer_current & 0x0fffffffu);

        recomp_runtime.registers.ecx = RECOMP_D3D_DEVICE_ADDRESS;
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        kick_off_adapter();
        passed &= expect_u32(
            "repeated kickoff channel base",
            *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x23bcu),
            0x001f2930u);
        passed &= expect_u32(
            "repeated kickoff channel PUT",
            *recomp_memory_u32(0x001f2970u),
            push_buffer_current & 0x0fffffffu);
        passed &= expect_u32(
            "repeated kickoff channel GET",
            *recomp_memory_u32(0x001f2974u),
            push_buffer_current & 0x0fffffffu);
        passed &= expect_u32(
            "repeated kickoff context count",
            *recomp_memory_u32(context),
            3u);
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x23bcu) =
            0xfd800000u;
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x0008u) = 3u;
    }

    reset_adapter = recomp_lookup_manual(0x001e3b00u);
    if (reset_adapter == NULL) {
        fprintf(stderr, "D3D creation: Reset manual lookup did not resolve\n");
        return 0;
    }
    {
        RecompD3dPresentationParameters reset_presentation = presentation;

        reset_presentation.full_screen_presentation_interval = 1u;
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 8u) = 0x00004001u;
        prepare_reset_call(call_memory, reset_presentation);
        reset_adapter();
        passed &= expect_u32(
            "reset HRESULT", recomp_runtime.registers.eax, RECOMP_D3D_OK);
        passed &= expect_u32(
            "reset ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 8u);
        passed &= expect_u32(
            "reset device flags",
            *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 8u),
            1u);
        passed &= expect_u32(
            "reset presentation interval",
            *recomp_memory_u32(0x001f2d84u),
            1u);
        passed &= expect_u32(
            "reset back-buffer surface 1",
            *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x21c4u),
            0x00a23ac8u);
        if (!recomp_d3d_presenter_memory_snapshot(&frame_snapshot)) {
            fprintf(stderr, "D3D creation: reset clear was unavailable\n");
            passed = 0;
        } else {
            passed &= expect_u32(
                "reset clear command count", frame_snapshot.command_count, 1u);
            passed &= expect_u32(
                "reset clear count", frame_snapshot.clear_count, 1u);
            passed &= expect_u32(
                "reset clear color",
                frame_snapshot.commands[0].data.clear.color,
                0u);
        }

        reset_presentation.full_screen_presentation_interval = 0x80000001u;
        prepare_reset_call(call_memory, reset_presentation);
        reset_adapter();
        passed &= expect_u32(
            "second observed reset HRESULT",
            recomp_runtime.registers.eax,
            RECOMP_D3D_OK);
        passed &= expect_u32(
            "second observed reset presentation interval",
            *recomp_memory_u32(0x001f2d84u),
            0x80000001u);

        reset_presentation.full_screen_presentation_interval = 2u;
        prepare_reset_call(call_memory, reset_presentation);
        reset_adapter();
        passed &= expect_u32(
            "unsupported reset HRESULT",
            recomp_runtime.registers.eax,
            RECOMP_D3D_INVALID_CALL);
        if (!recomp_d3d_presenter_memory_snapshot(&frame_snapshot)) {
            fprintf(stderr, "D3D creation: reset rejection lost presenter\n");
            passed = 0;
        } else {
            passed &= expect_u32(
                "rejected reset command count",
                frame_snapshot.command_count,
                2u);
        }
    }

    {
        uint32_t heap_checkpoint = xbox_HeapCheckpoint();

        prepare_call(call_memory, presentation, 0x40u);
        adapter();
        passed &= expect_u32(
            "duplicate create HRESULT",
            recomp_runtime.registers.eax,
            RECOMP_D3D_INVALID_CALL);
        passed &= expect_u32(
            "duplicate create heap rollback",
            xbox_HeapCheckpoint(),
            heap_checkpoint);
    }

    passed &= expect_u32(
        "push-buffer base",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x24u),
        TEST_HEAP_BASE + 0x1000u);
    passed &= expect_u32(
        "push-buffer current",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS),
        TEST_HEAP_BASE + 0x1000u +
            RECOMP_D3D_PUSH_BUFFER_INITIAL_OFFSET);
    passed &= expect_u32(
        "push-buffer limit",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 4u),
        TEST_HEAP_BASE + 0x1000u +
            RECOMP_D3D_PUSH_BUFFER_LIMIT_OFFSET);
    passed &= expect_u32(
        "context counter", *recomp_memory_u32(TEST_HEAP_BASE), 3u);
    passed &= expect_u32(
        "channel base",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x23bcu),
        0xfd800000u);
    passed &= expect_u32(
        "hardware base",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x23c0u),
        0xfd000000u);
    passed &= expect_u32(
        "back-buffer width",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0xa98u),
        0x2d0u);
    if (!recomp_d3d_presenter_memory_snapshot(&frame_snapshot)) {
        fprintf(stderr, "D3D creation: frame presenter was not initialized\n");
        passed = 0;
    } else {
        passed &= expect_u32(
            "frame presenter width", frame_snapshot.config.width, 720u);
        passed &= expect_u32(
            "frame presenter height", frame_snapshot.config.height, 480u);
        passed &= expect_u32(
            "frame presenter color format",
            frame_snapshot.config.color_format,
            RECOMP_D3D_PRESENTER_COLOR_FORMAT_BGRA8_UNORM);
        passed &= expect_u32(
            "frame presenter depth format",
            frame_snapshot.config.depth_format,
            RECOMP_D3D_PRESENTER_DEPTH_FORMAT_D24S8);
    }
    passed &= expect_u32(
        "initial swap counter",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x2c10u),
        0u);
    passed &= expect_u32(
        "initial pending fence 0",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x2124u),
        0u);
    passed &= expect_u32(
        "initial pending fence 1",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x2128u),
        0u);

    /* The implicit render-target / depth-stencil contract: the device must
       publish the game-owned descriptor addresses that the presentation
       parameters carried in, not addresses inside the memset device object. */
    passed &= expect_u32(
        "render-target surface (+0x21b4)",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x21b4u),
        0x00a23ab0u);
    passed &= expect_u32(
        "depth-stencil surface (+0x21b8)",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x21b8u),
        0x00a23ae0u);
    passed &= expect_u32(
        "back-buffer surface 0 (+0x21c0)",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x21c0u),
        0x00a23ab0u);
    passed &= expect_u32(
        "back-buffer surface 1 (+0x21c4)",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x21c4u),
        0x00a23ac8u);
    passed &= expect_u32(
        "back-buffer surface 2 (+0x21c8)",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x21c8u),
        0u);
    passed &= expect_u32(
        "depth-stencil copy (+0x21cc)",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x21ccu),
        0x00a23ae0u);
    passed &= expect_u32(
        "back-buffer count (+0x21bc)",
        *recomp_memory_u32(RECOMP_D3D_DEVICE_ADDRESS + 0x21bcu),
        2u);

    /* The game built the real 0x18-byte X_D3DSurface descriptors at the
       presentation addresses before CreateDevice; the adapter must publish
       pointers to them without overwriting the descriptor bytes. Map a
       dedicated region at 0x00A23AB0 and pre-fill the format dwords. */
    {
        static uint8_t descriptor_memory[0x48];
        const RecompMemoryRegion descriptor_regions[] = {
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
            {
                .address = TEST_HEAP_BASE,
                .size = sizeof heap_memory,
                .data = heap_memory,
            },
            {
                .address = 0x00a23ab0u,
                .size = sizeof descriptor_memory,
                .data = descriptor_memory,
            },
        };

        /* Register the descriptor region before running the adapter. */
        recomp_runtime_init(descriptor_regions, 4u, NULL, 0u, NULL, 0u);
        recomp_test_heap_reset(TEST_HEAP_BASE, -1);
        recomp_d3d_creation_adapter_reset();
        prepare_call(call_memory, presentation, 0x40u);
        /* Pre-fill the three game-owned descriptor format dwords. */
        *recomp_memory_u32(0x00a23ab0u + 0x0cu) = 0x00001200u;
        *recomp_memory_u32(0x00a23ac8u + 0x0cu) = 0x00001200u;
        *recomp_memory_u32(0x00a23ae0u + 0x0cu) = 0x00002e00u;
        adapter();
        passed &= expect_u32(
            "backbuffer[0] descriptor format preserved",
            *recomp_memory_u32(0x00a23ab0u + 0x0cu),
            0x00001200u);
        passed &= expect_u32(
            "backbuffer[1] descriptor format preserved",
            *recomp_memory_u32(0x00a23ac8u + 0x0cu),
            0x00001200u);
        passed &= expect_u32(
            "depth descriptor format preserved",
            *recomp_memory_u32(0x00a23ae0u + 0x0cu),
            0x00002e00u);
    }

    recomp_runtime_init(regions, 3u, NULL, 0u, NULL, 0u);
    recomp_test_heap_reset(TEST_HEAP_BASE, -1);
    recomp_d3d_creation_adapter_reset();
    prepare_call(call_memory, presentation, 0x41u);
    adapter();
    passed &= expect_u32(
        "failure HRESULT",
        recomp_runtime.registers.eax,
        RECOMP_D3D_INVALID_CALL);
    passed &= expect_u32(
        "failure ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 28u);
    passed &= expect_u32(
        "cleared failure output", *recomp_memory_u32(TEST_OUTPUT), 0u);

    return passed;
}
