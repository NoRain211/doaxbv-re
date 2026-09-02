#include <stdio.h>

#include "recomp_types.h"

/* An XMM register is 128 bits. The lifter used to declare it as a scalar host
   float, so every packed move transferred 4 of its 16 bytes and every packed
   ALU op was dropped to a comment. These check the replacement semantics that
   the generated code now depends on. */

static int expect_float(const char *check, float actual, float expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "sse: %s was %f, expected %f\n",
        check,
        (double)actual,
        (double)expected);
    return 0;
}
static int expect_u32(const char *check, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "sse: %s was 0x%08lx, expected 0x%08lx\n",
        check,
        (unsigned long)actual,
        (unsigned long)expected);
    return 0;
}

static RecompXmm row(float a, float b, float c, float d)
{
    RecompXmm value;

    value.f[0] = a;
    value.f[1] = b;
    value.f[2] = c;
    value.f[3] = d;
    return value;
}

int recomp_sse_semantics_test(void)
{
    int passed = 1;
    RecompXmm x = row(1.0f, 2.0f, 3.0f, 4.0f);
    RecompXmm mask;
    RecompXmm r;
    int i;

    /* shufps dst, src, 0 broadcasts lane 0. This is the multiplier in every
       matrix concatenation, and it used to execute as nothing. */
    r = XMM_SHUFFLE(x, x, 0);
    for (i = 0; i < 4; ++i) {
        passed &= expect_float("shufps broadcast", r.f[i], 1.0f);
    }

    /* 0x4E swaps halves, taking the low half from dst and high from src. */
    r = XMM_SHUFFLE(x, x, 0x4E);
    passed &= expect_float("shufps 0x4E lane0", r.f[0], 3.0f);
    passed &= expect_float("shufps 0x4E lane1", r.f[1], 4.0f);
    passed &= expect_float("shufps 0x4E lane2", r.f[2], 1.0f);
    passed &= expect_float("shufps 0x4E lane3", r.f[3], 2.0f);

    /* Packed arithmetic must touch all four lanes, not just lane 0. */
    r = XMM_MUL(x, row(2.0f, 2.0f, 2.0f, 2.0f));
    passed &= expect_float("mulps lane0", r.f[0], 2.0f);
    passed &= expect_float("mulps lane3", r.f[3], 8.0f);
    r = XMM_ADD(x, x);
    passed &= expect_float("addps lane2", r.f[2], 6.0f);
    r = XMM_SUB(x, row(1.0f, 1.0f, 1.0f, 1.0f));
    passed &= expect_float("subps lane1", r.f[1], 1.0f);
    r = XMM_DIV(x, row(2.0f, 2.0f, 2.0f, 2.0f));
    passed &= expect_float("divps lane3", r.f[3], 2.0f);

    /* andps with a sign mask is fabs. It is only correct on the integer
       lanes, which is why the union carries both views. */
    mask.u[0] = 0x7fffffffu;
    mask.u[1] = 0x7fffffffu;
    mask.u[2] = 0x7fffffffu;
    mask.u[3] = 0x7fffffffu;
    r = XMM_AND(row(-5.0f, -6.0f, 7.0f, -8.0f), mask);
    passed &= expect_float("andps fabs lane0", r.f[0], 5.0f);
    passed &= expect_float("andps fabs lane1", r.f[1], 6.0f);
    passed &= expect_float("andps fabs lane2", r.f[2], 7.0f);
    passed &= expect_float("andps fabs lane3", r.f[3], 8.0f);

    /* xorps with the sign bit negates. */
    mask.u[0] = 0x80000000u;
    mask.u[1] = 0x80000000u;
    mask.u[2] = 0x80000000u;
    mask.u[3] = 0x80000000u;
    r = XMM_XOR(x, mask);
    passed &= expect_float("xorps negate lane0", r.f[0], -1.0f);
    passed &= expect_float("xorps negate lane3", r.f[3], -4.0f);

    r = XMM_ANDN(mask, x);
    passed &= expect_float("andnps lane0", r.f[0], 1.0f);

    /* Comparison results are lane masks, not booleans. */
    r = XMM_CMP_LT(x, row(2.0f, 2.0f, 2.0f, 2.0f));
    passed &= expect_u32("cmpltps lane0", r.u[0], 0xffffffffu);
    passed &= expect_u32("cmpltps lane1", r.u[1], 0u);

    r = XMM_MIN(x, row(2.0f, 2.0f, 2.0f, 2.0f));
    passed &= expect_float("minps lane3", r.f[3], 2.0f);
    r = XMM_MAX(x, row(2.0f, 2.0f, 2.0f, 2.0f));
    passed &= expect_float("maxps lane0", r.f[0], 2.0f);

    r = XMM_UNPACK_LOW(x, row(5.0f, 6.0f, 7.0f, 8.0f));
    passed &= expect_float("unpcklps lane0", r.f[0], 1.0f);
    passed &= expect_float("unpcklps lane1", r.f[1], 5.0f);
    passed &= expect_float("unpcklps lane2", r.f[2], 2.0f);
    passed &= expect_float("unpcklps lane3", r.f[3], 6.0f);

    r = XMM_UNPACK_HIGH(x, row(5.0f, 6.0f, 7.0f, 8.0f));
    passed &= expect_float("unpckhps lane0", r.f[0], 3.0f);
    passed &= expect_float("unpckhps lane1", r.f[1], 7.0f);

    r = XMM_MOVE_LOW_TO_HIGH(x, row(5.0f, 6.0f, 7.0f, 8.0f));
    passed &= expect_float("movlhps lane0", r.f[0], 1.0f);
    passed &= expect_float("movlhps lane2", r.f[2], 5.0f);
    passed &= expect_float("movlhps lane3", r.f[3], 6.0f);

    r = XMM_MOVE_HIGH_TO_LOW(x, row(5.0f, 6.0f, 7.0f, 8.0f));
    passed &= expect_float("movhlps lane0", r.f[0], 7.0f);
    passed &= expect_float("movhlps lane2", r.f[2], 3.0f);

    /* movmskps used to return a hardcoded 0, and it feeds branches. */
    passed &= expect_u32(
        "movmskps", XMM_MOVEMASK(row(-1.0f, -1.0f, 1.0f, -1.0f)), 0xBu);
    passed &= expect_u32("movmskps positive", XMM_MOVEMASK(x), 0u);

    /* A scalar load zeroes bits 127:32. */
    r = XMM_SCALAR(9.0f);
    passed &= expect_float("movss lane0", r.f[0], 9.0f);
    passed &= expect_u32("movss lane1 zeroed", r.u[1], 0u);
    passed &= expect_u32("movss lane3 zeroed", r.u[3], 0u);

    r = XMM_ZERO();
    passed &= expect_u32("xorps self lane0", r.u[0], 0u);
    passed &= expect_u32("xorps self lane3", r.u[3], 0u);

    return passed;
}
