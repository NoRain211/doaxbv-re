#include "input_adapter.h"
#include "input_model.h"
#include "program_manual.h"
#include "runtime.h"

#include <stdio.h>
#include <string.h>

enum {
    TEST_STATIC_BASE = 0x00230000u,
    TEST_STATIC_SIZE = 0x00030000u,
    TEST_CALL_BASE = 0x29000000u,
    TEST_CALL_SIZE = 0x00001000u,
    TEST_ENTRY_ESP = TEST_CALL_BASE + 0x100u,
    TEST_GAMEPAD_TYPE = 0x00231e54u,
    TEST_OUTPUT = 0x00240000u,
};

static RecompInputGamepad sampled_gamepad;

static int expect_u32(const char *field, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "input model: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

static void prepare_call(uint32_t argument_count, const uint32_t *arguments)
{
    *recomp_memory_u32(TEST_ENTRY_ESP) = 0x0010abcdu;
    for (uint32_t i = 0u; i < argument_count; ++i) {
        *recomp_memory_u32(TEST_ENTRY_ESP + 4u + i * 4u) = arguments[i];
    }
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xa5a5a5a5u;
}

static bool sample_gamepad(RecompInputGamepad *gamepad)
{
    *gamepad = sampled_gamepad;
    return true;
}

int recomp_input_model_test(void)
{
    static uint8_t static_memory[TEST_STATIC_SIZE];
    static uint8_t call_memory[TEST_CALL_SIZE];
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
    };
    RecompInputModel plain;
    const RecompInputModel *model;
    uint32_t args[4];
    uint32_t handle;
    uint32_t insertions;
    uint32_t removals;
    int passed = 1;

    recomp_input_reset(&plain, 1u);
    passed &= expect_u32("plain devices", recomp_input_get_devices(&plain), 1u);
    passed &= expect_u32(
        "plain unchanged",
        recomp_input_get_device_changes(&plain, &insertions, &removals),
        0u);
    recomp_input_set_connected(&plain, 0u, false);
    passed &= expect_u32(
        "plain removal",
        recomp_input_get_device_changes(&plain, &insertions, &removals),
        1u);
    passed &= expect_u32("plain removed mask", removals, 1u);

    memset(static_memory, 0xa5, sizeof static_memory);
    memset(call_memory, 0, sizeof call_memory);
    memset(&sampled_gamepad, 0, sizeof sampled_gamepad);
    recomp_runtime_init(regions, 2u, NULL, 0u, NULL, 0u);
    recomp_input_adapter_reset();
    recomp_input_adapter_set_source(sample_gamepad);
    model = recomp_input_adapter_model();

    args[0] = TEST_GAMEPAD_TYPE;
    prepare_call(1u, args);
    recomp_input_lookup_manual(0x00232dc0u)();
    passed &= expect_u32("GetDevices mask", recomp_runtime.registers.eax, 1u);
    passed &= expect_u32(
        "GetDevices ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 8u);

    args[0] = TEST_GAMEPAD_TYPE;
    args[1] = 0u;
    args[2] = 0u;
    args[3] = 0u;
    prepare_call(4u, args);
    recomp_input_lookup_manual(0x00232e4fu)();
    handle = recomp_runtime.registers.eax;
    passed &= expect_u32("Open handle", handle, 0x58490001u);
    passed &= expect_u32(
        "Open ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 20u);

    args[0] = handle;
    args[1] = TEST_OUTPUT;
    prepare_call(2u, args);
    recomp_input_lookup_manual(0x00232eb1u)();
    passed &= expect_u32("Capabilities status", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32(
        "Capabilities subtype", *recomp_memory_i8(TEST_OUTPUT), 1u);
    passed &= expect_u32(
        "Capabilities tail", *recomp_memory_i8(TEST_OUTPUT + 24u), 0u);

    sampled_gamepad.buttons = 0x10u;
    sampled_gamepad.analog_buttons[2] = 0xffu;
    prepare_call(2u, args);
    recomp_input_lookup_manual(0x0023308fu)();
    passed &= expect_u32("State status", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32("State packet", *recomp_memory_u32(TEST_OUTPUT), 1u);
    passed &= expect_u32(
        "State buttons", *recomp_memory_u16(TEST_OUTPUT + 4u), 0x10u);
    passed &= expect_u32(
        "State analog A",
        (uint8_t)*recomp_memory_i8(TEST_OUTPUT + 8u),
        0xffu);
    passed &= expect_u32(
        "State ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 12u);
    prepare_call(2u, args);
    recomp_input_lookup_manual(0x0023308fu)();
    passed &= expect_u32(
        "Stable state packet", *recomp_memory_u32(TEST_OUTPUT), 1u);

    *recomp_memory_u16(TEST_OUTPUT + 0x42u) = 0x1234u;
    *recomp_memory_u16(TEST_OUTPUT + 0x44u) = 0x5678u;
    prepare_call(2u, args);
    recomp_input_lookup_manual(0x002330fbu)();
    passed &= expect_u32("SetState status", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32(
        "SetState left motor", model->ports[0].left_motor, 0x1234u);
    passed &= expect_u32(
        "SetState right motor", model->ports[0].right_motor, 0x5678u);

    args[0] = TEST_GAMEPAD_TYPE;
    args[1] = TEST_OUTPUT + 0x100u;
    args[2] = TEST_OUTPUT + 0x104u;
    prepare_call(3u, args);
    recomp_input_lookup_manual(0x00232de2u)();
    passed &= expect_u32("Changes status", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32(
        "Changes insertions", *recomp_memory_u32(args[1]), 0u);
    passed &= expect_u32("Changes removals", *recomp_memory_u32(args[2]), 0u);

    args[0] = handle;
    prepare_call(1u, args);
    recomp_input_lookup_manual(0x00232ea5u)();
    passed &= expect_u32("Close open flag", model->ports[0].open, 0u);
    passed &= expect_u32(
        "Close ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 8u);

    if (recomp_lookup_manual(0x00232dc0u) == NULL ||
        recomp_lookup_manual(0x002330fbu) == NULL ||
        recomp_input_lookup_manual(0x00232dbfu) != NULL ||
        recomp_input_lookup_manual(0x002330fcu) != NULL) {
        fprintf(stderr, "input model: manual lookup was not exact\n");
        passed = 0;
    }
    return passed;
}
