#include "crt_format_adapter.h"
#include "crt_format_model.h"
#include "runtime.h"

#include <stdio.h>
#include <string.h>

enum {
    TEST_MEMORY_BASE = 0x29000000u,
    TEST_MEMORY_SIZE = 0x00001000u,
    TEST_ENTRY_ESP = TEST_MEMORY_BASE + 0x100u,
    TEST_FORMAT = TEST_MEMORY_BASE + 0x200u,
    TEST_OUTPUT = TEST_MEMORY_BASE + 0x300u,
    TEST_VA_LIST = TEST_MEMORY_BASE + 0x400u,
    TEST_ARGUMENT = TEST_MEMORY_BASE + 0x500u,
    TEST_ARGUMENT_2 = TEST_MEMORY_BASE + 0x600u,
};

static uint8_t memory[TEST_MEMORY_SIZE];

typedef struct TestArguments {
    const RecompCrtFormatArgument *items;
    size_t count;
} TestArguments;

static int test_fetch(
    void *context,
    size_t index,
    char conversion,
    RecompCrtFormatArgument *argument)
{
    const TestArguments *arguments = context;

    (void)conversion;
    if (index >= arguments->count) {
        return 0;
    }
    *argument = arguments->items[index];
    return 1;
}

static int expect_u32(const char *field, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "CRT format: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

static int expect_model(
    const char *format,
    const RecompCrtFormatArgument *items,
    size_t count,
    size_t capacity,
    RecompCrtFormatResult expected_result,
    const char *expected)
{
    char output[64];
    TestArguments arguments = {items, count};
    size_t written = 0u;
    RecompCrtFormatResult result = recomp_crt_format(
        output, capacity, format, test_fetch, &arguments, &written);

    if (result != expected_result) {
        fprintf(stderr, "CRT format: '%s' result %d, expected %d\n",
            format, (int)result, (int)expected_result);
        return 0;
    }
    if (expected != NULL &&
        (strcmp(output, expected) != 0 || written != strlen(expected))) {
        fprintf(stderr, "CRT format: '%s' gave '%s', expected '%s'\n",
            format, output, expected);
        return 0;
    }
    return 1;
}

static void put_string(uint32_t address, const char *text)
{
    memcpy(memory + (address - TEST_MEMORY_BASE), text, strlen(text) + 1u);
}

/* Calls the adapter as guest vsprintf(TEST_OUTPUT, format, TEST_VA_LIST). */
static int expect_adapter(
    RecompFunction adapter,
    const char *format,
    const uint32_t *slots,
    uint32_t slot_count,
    const char *expected)
{
    int passed = 1;

    put_string(TEST_FORMAT, format);
    for (uint32_t i = 0u; i < slot_count; ++i) {
        *recomp_memory_u32(TEST_VA_LIST + i * 4u) = slots[i];
    }
    *recomp_memory_u32(TEST_ENTRY_ESP) = 0x0010abcdu;
    *recomp_memory_u32(TEST_ENTRY_ESP + 4u) = TEST_OUTPUT;
    *recomp_memory_u32(TEST_ENTRY_ESP + 8u) = TEST_FORMAT;
    *recomp_memory_u32(TEST_ENTRY_ESP + 12u) = TEST_VA_LIST;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    adapter();
    passed &= expect_u32(
        "adapter EAX", recomp_runtime.registers.eax, (uint32_t)strlen(expected));
    passed &= expect_u32(
        "adapter ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 4u);
    if (strcmp((char *)(memory + (TEST_OUTPUT - TEST_MEMORY_BASE)), expected)
        != 0) {
        fprintf(stderr, "CRT format: adapter '%s' gave '%s', expected '%s'\n",
            format, (char *)(memory + (TEST_OUTPUT - TEST_MEMORY_BASE)),
            expected);
        passed = 0;
    }
    return passed;
}

static int test_model(void)
{
    const RecompCrtFormatArgument numbers[] = {
        {(uint32_t)-7, NULL}, {5u, NULL}, {0xabu, NULL}, {0u, NULL}};
    const RecompCrtFormatArgument wma[] = {
        {0u, "E:\\TDATA\\"}, {1u, NULL}, {0x2au, NULL}};
    const RecompCrtFormatArgument text[] = {{0u, "ab"}};
    const RecompCrtFormatArgument five[] = {
        {1u, NULL}, {2u, NULL}, {3u, NULL}, {4u, NULL}, {5u, NULL}};
    const RecompCrtFormatArgument no_string[] = {{0u, NULL}};
    const RecompCrtFormatResult ok = RECOMP_CRT_FORMAT_OK;
    const RecompCrtFormatResult too_small = RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
    const RecompCrtFormatResult unsupported =
        RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE;
    const RecompCrtFormatResult invalid = RECOMP_CRT_FORMAT_INVALID_ARGUMENT;
    int passed = 1;

    passed &= expect_model("literal CRI diagnostic", numbers, 4u, 64u, ok,
        "literal CRI diagnostic");
    passed &= expect_model("100%%", numbers, 4u, 64u, ok, "100%");
    passed &= expect_model("min=%d", numbers, 4u, 64u, ok, "min=-7");
    passed &= expect_model("%3d:%02d", numbers, 4u, 64u, ok, " -7:05");
    passed &= expect_model("%2d/%d 100%% %c%04x", numbers, 4u, 64u, ok,
        "-7/5 100% \xab" "0000");
    passed &= expect_model("%s%04x\\%08x.bin", wma, 3u, 64u, ok,
        "E:\\TDATA\\0001\\0000002a.bin");
    passed &= expect_model("%u|%x|%i|%X", numbers, 4u, 64u, ok,
        "4294967289|5|171|0");
    passed &= expect_model("%x", numbers, 1u, 64u, ok, "fffffff9");
    passed &= expect_model("%i", numbers, 1u, 64u, ok, "-7");
    passed &= expect_model("%X", numbers + 2, 1u, 64u, ok, "AB");
    passed &= expect_model("%-5d|", numbers + 1, 1u, 64u, ok, "5    |");
    passed &= expect_model("%5s|", text, 1u, 64u, ok, "   ab|");
    passed &= expect_model("%d%d%d%d%d", five, 5u, 64u, ok, "12345");

    passed &= expect_model("min=%d", numbers, 4u, 7u, ok, "min=-7");
    passed &= expect_model("min=%d", numbers, 4u, 6u, too_small, NULL);
    passed &= expect_model("abcdef", numbers, 4u, 6u, too_small, NULL);
    passed &= expect_model("ab%%", numbers, 4u, 3u, too_small, NULL);
    passed &= expect_model("ab%%", numbers, 4u, 4u, ok, "ab%");

    passed &= expect_model("trailing %", numbers, 4u, 64u, unsupported, NULL);
    passed &= expect_model("%5", numbers, 4u, 64u, unsupported, NULL);
    passed &= expect_model("%.1f", numbers, 4u, 64u, unsupported, NULL);
    passed &= expect_model("%ld", numbers, 4u, 64u, unsupported, NULL);
    passed &= expect_model("%#s", text, 1u, 64u, unsupported, NULL);
    passed &= expect_model("%+u", numbers, 4u, 64u, unsupported, NULL);

    passed &= expect_model("%d%d", numbers, 1u, 64u, invalid, NULL);
    passed &= expect_model("%s", no_string, 1u, 64u, invalid, NULL);
    return passed;
}

int recomp_crt_format_adapter_test(void)
{
    const RecompMemoryRegion region = {
        .address = TEST_MEMORY_BASE,
        .size = sizeof memory,
        .data = memory,
    };
    RecompFunction adapter;
    int passed = test_model();

    memset(memory, 0, sizeof memory);
    recomp_runtime_init(&region, 1u, NULL, 0u, NULL, 0u);
    adapter = recomp_crt_format_lookup_manual(0x001ba67cu);
    passed &= expect_u32("lookup", adapter != NULL, 1u);
    if (adapter == NULL) {
        return 0;
    }
    put_string(TEST_ARGUMENT, "t:");
    put_string(TEST_ARGUMENT_2, "voice.afs");
    {
        const uint32_t strings[] = {TEST_ARGUMENT, TEST_ARGUMENT_2};
        const uint32_t string_decimal[] = {TEST_ARGUMENT, (uint32_t)-7};
        const uint32_t decimal[] = {(uint32_t)-7};
        const uint32_t decimal_string[] = {(uint32_t)-7, TEST_ARGUMENT};
        const uint32_t song[] = {TEST_ARGUMENT, 1u, 0x2au};

        passed &= expect_adapter(adapter, "literal CRI diagnostic", NULL, 0u,
            "literal CRI diagnostic");
        passed &= expect_adapter(adapter, "%s\\*", strings, 1u, "t:\\*");
        passed &= expect_adapter(adapter, "%s\\%s", strings, 2u,
            "t:\\voice.afs");
        passed &= expect_adapter(adapter, "E0010: Illigal parameter min=%d",
            decimal, 1u, "E0010: Illigal parameter min=-7");
        passed &= expect_adapter(adapter, "%s:%d", string_decimal, 2u,
            "t::-7");
        passed &= expect_adapter(adapter, "%s%04x\\%08x.bin", song, 3u,
            "t:0001\\0000002a.bin");
        passed &= expect_adapter(adapter, "%d%%%s", decimal_string, 2u,
            "-7%t:");
    }
    return passed;
}
