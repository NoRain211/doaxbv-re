#include "kernel_abi.h"

#include <stdio.h>
#include <string.h>

enum {
    TEST_MEMORY_BASE = 0x2a000000u,
    TEST_MEMORY_SIZE = 0x00001000u,
    TEST_ENTRY_ESP = TEST_MEMORY_BASE + 0x100u,
    TEST_RESULT = TEST_MEMORY_BASE + 0x200u,
};

static int expect_u32(const char *field, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "kernel video: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

static void prepare_stack(uint8_t *memory, uint32_t result)
{
    uint32_t *stack = (uint32_t *)(memory + TEST_ENTRY_ESP - TEST_MEMORY_BASE);

    stack[0] = 0x0010abcdu;
    stack[1] = 0xfd000000u;
    stack[2] = 0x0fu;
    stack[3] = 0u;
    stack[4] = result;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
}

static void prepare_getter_stack(uint8_t *memory)
{
    uint32_t *stack = (uint32_t *)(memory + TEST_ENTRY_ESP - TEST_MEMORY_BASE);

    stack[0] = 0x0010abcdu;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
}

int recomp_kernel_video_test(void)
{
    static uint8_t memory[TEST_MEMORY_SIZE];
    const RecompMemoryRegion region = {
        .address = TEST_MEMORY_BASE,
        .size = sizeof memory,
        .data = memory,
    };
    RecompFunction get_saved_data;
    RecompFunction send_option;
    uint32_t getter_stack_before;
    uint32_t stack_before[5];
    int passed = 1;

    memset(memory, 0xa5, sizeof memory);
    recomp_runtime_init(&region, 1u, NULL, 0u, NULL, 0u);
    get_saved_data = recomp_kernel_video(1u);
    send_option = recomp_kernel_video(2u);
    passed &= expect_u32("getter lookup", get_saved_data != NULL, 1u);
    passed &= expect_u32("send lookup", send_option != NULL, 1u);
    passed &= expect_u32(
        "previous ordinal", recomp_kernel_video(0u) != NULL, 0u);
    passed &= expect_u32(
        "next ordinal", recomp_kernel_video(3u) != NULL, 0u);
    if (get_saved_data == NULL || send_option == NULL) {
        return 0;
    }

    prepare_getter_stack(memory);
    getter_stack_before = *recomp_memory_u32(TEST_ENTRY_ESP);
    get_saved_data();
    passed &= expect_u32("getter value", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32(
        "getter ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 4u);
    passed &= expect_u32(
        "getter preserves stack",
        *recomp_memory_u32(TEST_ENTRY_ESP), getter_stack_before);

    prepare_stack(memory, TEST_RESULT);
    memcpy(stack_before, memory + TEST_ENTRY_ESP - TEST_MEMORY_BASE, 20u);
    *recomp_memory_u32(TEST_RESULT) = 0xa5a5a5a5u;
    send_option();
    passed &= expect_u32("status", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32(
        "ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 20u);
    passed &= expect_u32("result", *recomp_memory_u32(TEST_RESULT), 0u);
    passed &= expect_u32(
        "preserves stack",
        memcmp(stack_before, memory + TEST_ENTRY_ESP - TEST_MEMORY_BASE, 20u),
        0u);

    prepare_stack(memory, 0u);
    send_option();
    passed &= expect_u32("null result status", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32(
        "null result ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 20u);

    return passed;
}
