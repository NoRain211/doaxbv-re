#include "d3d_frame_adapter.h"
#include "d3d_presenter_memory_test.h"
#include "program_manual.h"
#include "runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    TEST_DEVICE_BASE = 0x001f0000u,
    TEST_DEVICE_SIZE = 0x00007000u,
    TEST_CALL_BASE = 0x28000000u,
    TEST_CALL_SIZE = 0x00001000u,
    TEST_ENTRY_ESP = TEST_CALL_BASE + 0x100u,
    TEST_DEVICE = 0x001f3120u,
    TEST_KERNEL_DATA_BASE = 0x00740000u,
    TEST_KERNEL_DATA_SIZE = 0x00001000u,
    TEST_KE_TICK_COUNT = TEST_KERNEL_DATA_BASE + 0x40u,
};

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
        "D3D frame adapter: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

static void prepare_stack(uint8_t *call_memory, const uint32_t *args, size_t count)
{
    uint32_t *stack = (uint32_t *)(void *)(
        call_memory + TEST_ENTRY_ESP - TEST_CALL_BASE);

    memset(call_memory, 0, TEST_CALL_SIZE);
    stack[0] = 0xdeadbeefu;
    for (size_t i = 0u; i < count; ++i) {
        stack[i + 1u] = args[i];
    }
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
}

int recomp_d3d_frame_adapter_test(void)
{
    static uint8_t device_memory[TEST_DEVICE_SIZE];
    static uint8_t call_memory[TEST_CALL_SIZE];
    static uint8_t kernel_data_memory[TEST_KERNEL_DATA_SIZE];
    const RecompMemoryRegion regions[] = {
        {
            .address = TEST_DEVICE_BASE,
            .size = sizeof device_memory,
            .data = device_memory,
        },
        {
            .address = TEST_CALL_BASE,
            .size = sizeof call_memory,
            .data = call_memory,
        },
        {
            .address = TEST_KERNEL_DATA_BASE,
            .size = sizeof kernel_data_memory,
            .data = kernel_data_memory,
        },
    };
    const uint32_t clear_args[] = {
        0u,
        0u,
        0xf3u,
        0x10203040u,
        0x3f800000u,
        0x2au,
    };
    const uint32_t swap_args[] = {0u};
    const RecompD3dPresenterConfig config = {
        .width = 720u,
        .height = 480u,
        .color_format = RECOMP_D3D_PRESENTER_COLOR_FORMAT_BGRA8_UNORM,
        .depth_format = RECOMP_D3D_PRESENTER_DEPTH_FORMAT_D24S8,
    };
    RecompD3dPresenterMemorySnapshot snapshot;
    RecompFunction clear;
    RecompFunction swap;
    uint32_t clear_z_bits;
    int passed = 1;

    memset(device_memory, 0, sizeof device_memory);
    memset(kernel_data_memory, 0, sizeof kernel_data_memory);
    recomp_runtime_init(regions, 3u, NULL, 0u, NULL, 0u);
    recomp_d3d_frame_adapter_reset();
    recomp_d3d_frame_adapter_initialize(&config, TEST_DEVICE);

    clear = recomp_d3d_frame_lookup_manual(0x001e72d0u);
    swap = recomp_d3d_frame_lookup_manual(0x001e8f30u);
    if (clear == NULL || swap == NULL) {
        fprintf(stderr, "D3D frame adapter: exact lookup failed\n");
        return 0;
    }
    if (recomp_d3d_frame_lookup_manual(0x001e72cfu) != NULL ||
        recomp_d3d_frame_lookup_manual(0x001e72d1u) != NULL ||
        recomp_d3d_frame_lookup_manual(0x001e8f2fu) != NULL ||
        recomp_d3d_frame_lookup_manual(0x001e8f31u) != NULL) {
        fprintf(stderr, "D3D frame adapter: adjacent lookup resolved\n");
        passed = 0;
    }
    if (recomp_lookup_manual(0x001e72d0u) != clear ||
        recomp_lookup_manual(0x001e8f30u) != swap) {
        fprintf(stderr, "D3D frame adapter: manual lookup chain failed\n");
        passed = 0;
    }

    prepare_stack(call_memory, clear_args, 6u);
    recomp_runtime.registers.eax = 0xa5a5a5a5u;
    clear();
    passed &= expect_u32(
        "Clear ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 28u);
    passed &= expect_u32(
        "Clear EAX", recomp_runtime.registers.eax, 0xa5a5a5a5u);
    if (!recomp_d3d_presenter_memory_snapshot(&snapshot)) {
        fprintf(stderr, "D3D frame adapter: Clear snapshot unavailable\n");
        passed = 0;
    } else {
        passed &= expect_u32("Clear command count", snapshot.command_count, 1u);
        passed &= expect_u32(
            "Clear command type",
            snapshot.commands[0].type,
            RECOMP_D3D_PRESENTER_COMMAND_CLEAR);
        passed &= expect_u32(
            "Clear color",
            snapshot.commands[0].data.clear.color,
            0x10203040u);
        memcpy(
            &clear_z_bits,
            &snapshot.commands[0].data.clear.z,
            sizeof clear_z_bits);
        passed &= expect_u32("Clear z bits", clear_z_bits, 0x3f800000u);
        passed &= expect_u32(
            "Clear stencil", snapshot.commands[0].data.clear.stencil, 0x2au);
    }

    prepare_stack(call_memory, swap_args, 1u);
    recomp_runtime.registers.eax = 0xccccccccu;
    swap();
    passed &= expect_u32(
        "Swap ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 8u);
    passed &= expect_u32("Swap EAX", recomp_runtime.registers.eax, 1u);
    passed &= expect_u32(
        "guest swap counter",
        *recomp_memory_u32(TEST_DEVICE + 0x2c10u),
        1u);
    passed &= expect_u32(
        "KeTickCount",
        *recomp_memory_u32(TEST_KE_TICK_COUNT),
        16u);
    if (!recomp_d3d_presenter_memory_snapshot(&snapshot)) {
        fprintf(stderr, "D3D frame adapter: Swap snapshot unavailable\n");
        passed = 0;
    } else {
        passed &= expect_u32("Swap command count", snapshot.command_count, 2u);
        passed &= expect_u32(
            "Swap effective flags",
            snapshot.commands[1].data.present.effective_flags,
            5u);
        passed &= expect_u32(
            "Swap command counter",
            snapshot.commands[1].data.present.swap_counter,
            1u);
    }

    recomp_d3d_frame_adapter_reset();
    if (recomp_d3d_presenter_memory_snapshot(&snapshot)) {
        fprintf(stderr, "D3D frame adapter: reset left a presenter\n");
        passed = 0;
    }
    return passed;
}
