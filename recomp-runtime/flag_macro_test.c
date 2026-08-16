#include <stdio.h>

#include "recomp_types.h"

/* x86 sets SF from the top bit of the operand width, not from bit 31. The
   generated code applies the flag macros to LO8/HI8/LO16 sub-register reads,
   so the macros have to recover the sign at that width. */

static int expect_true(const char *check, int actual)
{
    if (actual) {
        return 1;
    }
    fprintf(stderr, "flag macros: %s was false, expected true\n", check);
    return 0;
}

static int expect_false(const char *check, int actual)
{
    if (!actual) {
        return 1;
    }
    fprintf(stderr, "flag macros: %s was true, expected false\n", check);
    return 0;
}

static uint8_t read_u8_once(uint8_t value, int *read_count)
{
    *read_count += 1;
    return value;
}

static int expect_count(const char *check, int expected, int actual)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "flag macros: %s was %d, expected %d\n",
        check,
        actual,
        expected);
    return 0;
}

int recomp_flag_macro_test(void)
{
    int passed = 1;
    int left_reads = 0;
    int right_reads = 0;

    /* The observed D3D case: ebx = 0xf8, lifted from "test al,al; js". */
    passed &= expect_true(
        "TEST_S(LO8(0xf8), LO8(0xf8))", TEST_S(LO8(0xf8u), LO8(0xf8u)));
    passed &= expect_false(
        "TEST_S(LO8(0x78), LO8(0x78))", TEST_S(LO8(0x78u), LO8(0x78u)));
    passed &= expect_true(
        "TEST_S(HI8(0x8000), HI8(0x8000))",
        TEST_S(HI8(0x8000u), HI8(0x8000u)));
    passed &= expect_true(
        "TEST_S(LO16(0x8000), LO16(0x8000))",
        TEST_S(LO16(0x8000u), LO16(0x8000u)));
    passed &= expect_false(
        "TEST_S(LO16(0x7fff), LO16(0x7fff))",
        TEST_S(LO16(0x7fffu), LO16(0x7fffu)));

    /* A full-width operand still takes its sign from bit 31. */
    passed &= expect_true(
        "TEST_S(0x80000000, 0x80000000)",
        TEST_S(0x80000000u, 0x80000000u));
    passed &= expect_false(
        "TEST_S(0x000000f8, 0x000000f8)", TEST_S(0xf8u, 0xf8u));

    /* Signed compares of sub-register reads follow the same rule. */
    passed &= expect_true(
        "CMP_L(LO8(0xf8), LO8(0x01))", CMP_L(LO8(0xf8u), LO8(0x01u)));
    passed &= expect_false(
        "CMP_L(LO8(0x01), LO8(0xf8))", CMP_L(LO8(0x01u), LO8(0xf8u)));
    passed &= expect_true(
        "CMP_GE(LO8(0x01), LO8(0xf8))", CMP_GE(LO8(0x01u), LO8(0xf8u)));
    passed &= expect_true(
        "CMP_G(LO16(0x0001), LO16(0xffff))",
        CMP_G(LO16(0x0001u), LO16(0xffffu)));
    passed &= expect_true(
        "CMP_LE(LO16(0xffff), LO16(0x0001))",
        CMP_LE(LO16(0xffffu), LO16(0x0001u)));

    /* Full-width signed compares are unchanged. */
    passed &= expect_true("CMP_L(0xfffffff8, 1)", CMP_L(0xfffffff8u, 1u));
    passed &= expect_false("CMP_L(1, 0xfffffff8)", CMP_L(1u, 0xfffffff8u));

    /* Generated memory operands are function calls, so flag macros must not
       evaluate either operand more than once. */
    passed &= expect_true(
        "CMP_L single evaluation",
        CMP_L(
            read_u8_once(0xf8u, &left_reads),
            read_u8_once(0x01u, &right_reads)));
    passed &= expect_count("CMP_L left reads", 1, left_reads);
    passed &= expect_count("CMP_L right reads", 1, right_reads);

    left_reads = 0;
    right_reads = 0;
    passed &= expect_true(
        "TEST_S single evaluation",
        TEST_S(
            read_u8_once(0x80u, &left_reads),
            read_u8_once(0x80u, &right_reads)));
    passed &= expect_count("TEST_S left reads", 1, left_reads);
    passed &= expect_count("TEST_S right reads", 1, right_reads);

    if (!passed) {
        return 0;
    }
    puts("recomp runtime: sub-register flag macros carry the operand sign");
    return 1;
}
