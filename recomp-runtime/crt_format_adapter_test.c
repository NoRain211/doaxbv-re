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

int recomp_crt_format_adapter_test(void)
{
    static uint8_t memory[TEST_MEMORY_SIZE];
    const RecompMemoryRegion region = {
        .address = TEST_MEMORY_BASE,
        .size = sizeof memory,
        .data = memory,
    };
    const char literal[] = "literal CRI diagnostic";
    const char string_format[] = "%s\\*";
    const char string_argument[] = "t:";
    const char string_expected[] = "t:\\*";
    const char two_string_format[] = "%s\\%s";
    const char second_string_argument[] = "voice.afs";
    const char two_string_expected[] = "t:\\voice.afs";
    const char decimal_format[] = "E0010: Illigal parameter min=%d";
    const char decimal_expected[] = "E0010: Illigal parameter min=-7";
    const char string_decimal_format[] = "%s:%d";
    const char string_decimal_expected[] = "t::-7";
    const uint32_t literal_length = (uint32_t)(sizeof literal - 1u);
    const int32_t decimal_argument = -7;
    char output[32];
    size_t written = 0u;
    RecompFunction adapter;
    int passed = 1;

    passed &= expect_u32(
        "plain literal",
        recomp_crt_format_literal(
            output, sizeof output, literal, &written),
        RECOMP_CRT_FORMAT_OK);
    passed &= expect_u32("plain length", (uint32_t)written, literal_length);
    passed &= expect_u32(
        "plain contents", strcmp(output, literal) == 0, 1u);
    passed &= expect_u32(
        "directive rejection",
        recomp_crt_format_literal(
            output, sizeof output, "value=%u", &written),
        RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE);
    passed &= expect_u32(
        "one string",
        recomp_crt_format_one_string(
            output,
            sizeof output,
            string_format,
            string_argument,
            &written),
        RECOMP_CRT_FORMAT_OK);
    passed &= expect_u32(
        "one string length",
        (uint32_t)written,
        (uint32_t)(sizeof string_expected - 1u));
    passed &= expect_u32(
        "one string contents", strcmp(output, string_expected) == 0, 1u);
    passed &= expect_u32(
        "second directive rejection",
        recomp_crt_format_one_string(
            output, sizeof output, "%s%s", string_argument, &written),
        RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE);
    passed &= expect_u32(
        "two strings",
        recomp_crt_format_two_strings(
            output,
            sizeof output,
            two_string_format,
            string_argument,
            second_string_argument,
            &written),
        RECOMP_CRT_FORMAT_OK);
    passed &= expect_u32(
        "two string length",
        (uint32_t)written,
        (uint32_t)(sizeof two_string_expected - 1u));
    passed &= expect_u32(
        "two string contents", strcmp(output, two_string_expected) == 0, 1u);
    passed &= expect_u32(
        "third directive rejection",
        recomp_crt_format_two_strings(
            output,
            sizeof output,
            "%s%s%s",
            string_argument,
            second_string_argument,
            &written),
        RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE);
    passed &= expect_u32(
        "one signed decimal",
        recomp_crt_format_one_signed_decimal(
            output,
            sizeof output,
            decimal_format,
            decimal_argument,
            &written),
        RECOMP_CRT_FORMAT_OK);
    passed &= expect_u32(
        "one signed decimal length",
        (uint32_t)written,
        (uint32_t)(sizeof decimal_expected - 1u));
    passed &= expect_u32(
        "one signed decimal contents", strcmp(output, decimal_expected) == 0, 1u);
    passed &= expect_u32(
        "signed decimal output too small",
        recomp_crt_format_one_signed_decimal(
            output,
            sizeof decimal_expected - 1u,
            decimal_format,
            decimal_argument,
            &written),
        RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL);
    passed &= expect_u32(
        "signed decimal second directive rejection",
        recomp_crt_format_one_signed_decimal(
            output,
            sizeof output,
            "%d min=%d",
            decimal_argument,
            &written),
        RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE);
    passed &= expect_u32(
        "string signed decimal",
        recomp_crt_format_string_signed_decimal(
            output,
            sizeof output,
            string_decimal_format,
            string_argument,
            decimal_argument,
            &written),
        RECOMP_CRT_FORMAT_OK);
    passed &= expect_u32(
        "string signed decimal length",
        (uint32_t)written,
        (uint32_t)(sizeof string_decimal_expected - 1u));
    passed &= expect_u32(
        "string signed decimal contents",
        strcmp(output, string_decimal_expected) == 0,
        1u);
    passed &= expect_u32(
        "string signed decimal output too small",
        recomp_crt_format_string_signed_decimal(
            output,
            sizeof string_decimal_expected - 1u,
            string_decimal_format,
            string_argument,
            decimal_argument,
            &written),
        RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL);
    passed &= expect_u32(
        "string signed decimal third directive rejection",
        recomp_crt_format_string_signed_decimal(
            output,
            sizeof output,
            "%s:%d:%s",
            string_argument,
            decimal_argument,
            &written),
        RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE);
    passed &= expect_u32(
        "string signed decimal missing decimal rejection",
        recomp_crt_format_string_signed_decimal(
            output,
            sizeof output,
            "%s%s",
            string_argument,
            decimal_argument,
            &written),
        RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE);
    passed &= expect_u32(
        "string signed decimal order rejection",
        recomp_crt_format_string_signed_decimal(
            output,
            sizeof output,
            "%d%s",
            string_argument,
            decimal_argument,
            &written),
        RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE);

    memset(memory, 0, sizeof memory);
    memcpy(memory + (TEST_FORMAT - TEST_MEMORY_BASE), literal, sizeof literal);
    recomp_runtime_init(&region, 1u, NULL, 0u, NULL, 0u);
    *recomp_memory_u32(TEST_ENTRY_ESP) = 0x0010abcdu;
    *recomp_memory_u32(TEST_ENTRY_ESP + 4u) = TEST_OUTPUT;
    *recomp_memory_u32(TEST_ENTRY_ESP + 8u) = TEST_FORMAT;
    *recomp_memory_u32(TEST_ENTRY_ESP + 12u) = TEST_MEMORY_BASE + 0x400u;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;

    adapter = recomp_crt_format_lookup_manual(0x001ba67cu);
    passed &= expect_u32("lookup", adapter != NULL, 1u);
    if (adapter != NULL) {
        adapter();
        passed &= expect_u32(
            "adapter EAX", recomp_runtime.registers.eax, literal_length);
        passed &= expect_u32(
            "adapter ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 4u);
        passed &= expect_u32(
            "adapter contents",
            strcmp(
                (char *)(memory + (TEST_OUTPUT - TEST_MEMORY_BASE)),
                literal) == 0,
            1u);
    }

    memset(memory, 0, sizeof memory);
    memcpy(
        memory + (TEST_FORMAT - TEST_MEMORY_BASE),
        string_format,
        sizeof string_format);
    memcpy(
        memory + (TEST_ARGUMENT - TEST_MEMORY_BASE),
        string_argument,
        sizeof string_argument);
    *recomp_memory_u32(TEST_VA_LIST) = TEST_ARGUMENT;
    *recomp_memory_u32(TEST_ENTRY_ESP) = 0x0010abcdu;
    *recomp_memory_u32(TEST_ENTRY_ESP + 4u) = TEST_OUTPUT;
    *recomp_memory_u32(TEST_ENTRY_ESP + 8u) = TEST_FORMAT;
    *recomp_memory_u32(TEST_ENTRY_ESP + 12u) = TEST_VA_LIST;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    if (adapter != NULL) {
        adapter();
        passed &= expect_u32(
            "string adapter EAX",
            recomp_runtime.registers.eax,
            (uint32_t)(sizeof string_expected - 1u));
        passed &= expect_u32(
            "string adapter ESP",
            recomp_runtime.registers.esp,
            TEST_ENTRY_ESP + 4u);
        passed &= expect_u32(
            "string adapter contents",
            strcmp(
                (char *)(memory + (TEST_OUTPUT - TEST_MEMORY_BASE)),
                string_expected) == 0,
            1u);
    }

    memset(memory, 0, sizeof memory);
    memcpy(
        memory + (TEST_FORMAT - TEST_MEMORY_BASE),
        two_string_format,
        sizeof two_string_format);
    memcpy(
        memory + (TEST_ARGUMENT - TEST_MEMORY_BASE),
        string_argument,
        sizeof string_argument);
    memcpy(
        memory + (TEST_ARGUMENT_2 - TEST_MEMORY_BASE),
        second_string_argument,
        sizeof second_string_argument);
    *recomp_memory_u32(TEST_VA_LIST) = TEST_ARGUMENT;
    *recomp_memory_u32(TEST_VA_LIST + 4u) = TEST_ARGUMENT_2;
    *recomp_memory_u32(TEST_ENTRY_ESP) = 0x0010abcdu;
    *recomp_memory_u32(TEST_ENTRY_ESP + 4u) = TEST_OUTPUT;
    *recomp_memory_u32(TEST_ENTRY_ESP + 8u) = TEST_FORMAT;
    *recomp_memory_u32(TEST_ENTRY_ESP + 12u) = TEST_VA_LIST;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    if (adapter != NULL) {
        adapter();
        passed &= expect_u32(
            "two string adapter EAX",
            recomp_runtime.registers.eax,
            (uint32_t)(sizeof two_string_expected - 1u));
        passed &= expect_u32(
            "two string adapter ESP",
            recomp_runtime.registers.esp,
            TEST_ENTRY_ESP + 4u);
        passed &= expect_u32(
            "two string adapter contents",
            strcmp(
                (char *)(memory + (TEST_OUTPUT - TEST_MEMORY_BASE)),
                two_string_expected) == 0,
            1u);
    }

    memset(memory, 0, sizeof memory);
    memcpy(
        memory + (TEST_FORMAT - TEST_MEMORY_BASE),
        decimal_format,
        sizeof decimal_format);
    *recomp_memory_u32(TEST_VA_LIST) = (uint32_t)decimal_argument;
    *recomp_memory_u32(TEST_ENTRY_ESP) = 0x0010abcdu;
    *recomp_memory_u32(TEST_ENTRY_ESP + 4u) = TEST_OUTPUT;
    *recomp_memory_u32(TEST_ENTRY_ESP + 8u) = TEST_FORMAT;
    *recomp_memory_u32(TEST_ENTRY_ESP + 12u) = TEST_VA_LIST;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    if (adapter != NULL) {
        adapter();
        passed &= expect_u32(
            "signed decimal adapter EAX",
            recomp_runtime.registers.eax,
            (uint32_t)(sizeof decimal_expected - 1u));
        passed &= expect_u32(
            "signed decimal adapter ESP",
            recomp_runtime.registers.esp,
            TEST_ENTRY_ESP + 4u);
        passed &= expect_u32(
            "signed decimal adapter contents",
            strcmp(
                (char *)(memory + (TEST_OUTPUT - TEST_MEMORY_BASE)),
                decimal_expected) == 0,
            1u);
    }

    memset(memory, 0, sizeof memory);
    memcpy(
        memory + (TEST_FORMAT - TEST_MEMORY_BASE),
        string_decimal_format,
        sizeof string_decimal_format);
    memcpy(
        memory + (TEST_ARGUMENT - TEST_MEMORY_BASE),
        string_argument,
        sizeof string_argument);
    *recomp_memory_u32(TEST_VA_LIST) = TEST_ARGUMENT;
    *recomp_memory_u32(TEST_VA_LIST + 4u) = (uint32_t)decimal_argument;
    *recomp_memory_u32(TEST_ENTRY_ESP) = 0x0010abcdu;
    *recomp_memory_u32(TEST_ENTRY_ESP + 4u) = TEST_OUTPUT;
    *recomp_memory_u32(TEST_ENTRY_ESP + 8u) = TEST_FORMAT;
    *recomp_memory_u32(TEST_ENTRY_ESP + 12u) = TEST_VA_LIST;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    if (adapter != NULL) {
        adapter();
        passed &= expect_u32(
            "string signed decimal adapter EAX",
            recomp_runtime.registers.eax,
            (uint32_t)(sizeof string_decimal_expected - 1u));
        passed &= expect_u32(
            "string signed decimal adapter ESP",
            recomp_runtime.registers.esp,
            TEST_ENTRY_ESP + 4u);
        passed &= expect_u32(
            "string signed decimal adapter contents",
            strcmp(
                (char *)(memory + (TEST_OUTPUT - TEST_MEMORY_BASE)),
                string_decimal_expected) == 0,
            1u);
    }
    return passed;
}
