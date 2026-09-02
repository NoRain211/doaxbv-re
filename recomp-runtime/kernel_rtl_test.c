#include "kernel_abi.h"

#include <stdio.h>
#include <string.h>

enum {
    TEST_MEMORY_BASE = 0x2a000000u,
    TEST_MEMORY_SIZE = 0x00001000u,
    TEST_ENTRY_ESP = TEST_MEMORY_BASE + 0x100u,
    TEST_TIME = TEST_MEMORY_BASE + 0x200u,
    TEST_FIELDS = TEST_MEMORY_BASE + 0x300u,
};

/* Expectations were produced outside this translation unit, from a calendar
   implementation that shares no code with the one under test. */
typedef struct TimeVector {
    uint64_t time;
    uint16_t fields[RECOMP_TIME_FIELD_COUNT];
} TimeVector;

static const TimeVector vectors[] = {
    {0x0000000000000000ull, {1601u, 1u, 1u, 0u, 0u, 0u, 0u, 1u}},
    {0x019db1ded53e8000ull, {1970u, 1u, 1u, 0u, 0u, 0u, 0u, 4u}},
    {0x01bf82b162c9fc50ull, {2000u, 2u, 29u, 12u, 34u, 56u, 789u, 2u}},
    {0x014f6598c43f8000ull, {1900u, 3u, 1u, 0u, 0u, 0u, 0u, 4u}},
    {0x01dd36a5e8068cb0ull, {2026u, 8u, 28u, 4u, 30u, 8u, 123u, 5u}},
    {0x01c4ef94d62258f0ull, {2004u, 12u, 31u, 23u, 59u, 59u, 999u, 5u}},
    {0x022f9fc03dc34000ull, {2100u, 3u, 1u, 0u, 0u, 0u, 0u, 1u}},
    {0x00038ad130b38000ull, {1604u, 2u, 29u, 0u, 0u, 0u, 0u, 0u}},
};

static const char *const field_names[RECOMP_TIME_FIELD_COUNT] = {
    "year", "month", "day", "hour", "minute", "second", "millisecond",
    "weekday"
};

static int expect_u32(const char *field, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "kernel rtl: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

static int check_fields(
    const char *label,
    const uint16_t *actual,
    const uint16_t *expected)
{
    int passed = 1;

    for (unsigned i = 0u; i < RECOMP_TIME_FIELD_COUNT; ++i) {
        if (actual[i] != expected[i]) {
            fprintf(
                stderr,
                "kernel rtl: %s %s was %u, expected %u\n",
                label,
                field_names[i],
                (unsigned)actual[i],
                (unsigned)expected[i]);
            passed = 0;
        }
    }
    return passed;
}

static int test_conversion_model(void)
{
    int passed = 1;

    for (unsigned i = 0u; i < sizeof vectors / sizeof vectors[0]; ++i) {
        uint16_t fields[RECOMP_TIME_FIELD_COUNT];
        uint64_t time = 0u;

        memset(fields, 0xa5, sizeof fields);
        if (!recomp_kernel_time_to_time_fields(vectors[i].time, fields)) {
            fprintf(stderr, "kernel rtl: vector %u rejected\n", i);
            passed = 0;
            continue;
        }
        passed &= check_fields("forward", fields, vectors[i].fields);

        /* The round trip pins the inverse without a second expectation
           table: whatever the forward direction produced must convert back
           to the tick count it came from. */
        if (!recomp_kernel_time_fields_to_time(vectors[i].fields, &time)) {
            fprintf(stderr, "kernel rtl: vector %u inverse rejected\n", i);
            passed = 0;
            continue;
        }
        passed &= expect_u32(
            "inverse low", (uint32_t)time, (uint32_t)vectors[i].time);
        passed &= expect_u32(
            "inverse high",
            (uint32_t)(time >> 32u),
            (uint32_t)(vectors[i].time >> 32u));
    }

    /* Out-of-range fields must be refused rather than silently normalised,
       because the guest branches on the BOOLEAN. */
    {
        uint16_t february30[RECOMP_TIME_FIELD_COUNT] =
            {2001u, 2u, 29u, 0u, 0u, 0u, 0u, 0u};
        uint16_t before_epoch[RECOMP_TIME_FIELD_COUNT] =
            {1600u, 12u, 31u, 0u, 0u, 0u, 0u, 0u};
        uint16_t bad_hour[RECOMP_TIME_FIELD_COUNT] =
            {2004u, 1u, 1u, 24u, 0u, 0u, 0u, 0u};
        uint64_t time = 0x5a5a5a5au;

        passed &= expect_u32(
            "rejects february 29 in a common year",
            (uint32_t)recomp_kernel_time_fields_to_time(february30, &time),
            0u);
        passed &= expect_u32(
            "rejects a year before the epoch",
            (uint32_t)recomp_kernel_time_fields_to_time(before_epoch, &time),
            0u);
        passed &= expect_u32(
            "rejects hour 24",
            (uint32_t)recomp_kernel_time_fields_to_time(bad_hour, &time),
            0u);
        passed &= expect_u32(
            "rejection leaves the output alone", (uint32_t)time, 0x5a5a5a5au);
    }
    return passed;
}

int recomp_kernel_rtl_test(void)
{
    static uint8_t memory[TEST_MEMORY_SIZE];
    const RecompMemoryRegion region = {
        .address = TEST_MEMORY_BASE,
        .size = sizeof memory,
        .data = memory,
    };
    RecompFunction to_fields;
    RecompFunction to_time;
    const TimeVector *vector = &vectors[4];
    uint16_t observed[RECOMP_TIME_FIELD_COUNT];
    uint32_t *stack;
    int passed = 1;

    memset(memory, 0xa5, sizeof memory);
    recomp_runtime_init(&region, 1u, NULL, 0u, NULL, 0u);

    passed &= test_conversion_model();

    to_time = recomp_kernel_rtl(304u);
    to_fields = recomp_kernel_rtl(305u);
    passed &= expect_u32("ordinal 304 lookup", to_time != NULL, 1u);
    passed &= expect_u32("ordinal 305 lookup", to_fields != NULL, 1u);
    passed &= expect_u32(
        "ordinal 303 stays unimplemented", recomp_kernel_rtl(303u) != NULL, 0u);
    passed &= expect_u32(
        "ordinal 306 stays unimplemented", recomp_kernel_rtl(306u) != NULL, 0u);
    if (to_time == NULL || to_fields == NULL) {
        return 0;
    }

    /* Both imports are stdcall with two arguments, so each must pop twelve
       bytes. Getting that wrong corrupts ESP and fails somewhere unrelated. */
    stack = (uint32_t *)(memory + TEST_ENTRY_ESP - TEST_MEMORY_BASE);
    stack[0] = 0x0010abcdu;
    stack[1] = TEST_TIME;
    stack[2] = TEST_FIELDS;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    *recomp_memory_u32(TEST_TIME) = (uint32_t)vector->time;
    *recomp_memory_u32(TEST_TIME + 4u) = (uint32_t)(vector->time >> 32u);
    to_fields();
    for (unsigned i = 0u; i < RECOMP_TIME_FIELD_COUNT; ++i) {
        observed[i] = *recomp_memory_u16(TEST_FIELDS + 2u * i);
    }
    passed &= check_fields("bridge", observed, vector->fields);
    passed &= expect_u32(
        "to-fields ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 12u);
    passed &= expect_u32(
        "to-fields return address survives",
        *recomp_memory_u32(TEST_ENTRY_ESP),
        0x0010abcdu);

    /* Feed the fields the first bridge wrote straight back into the second. */
    stack[0] = 0x0010abcdu;
    stack[1] = TEST_FIELDS;
    stack[2] = TEST_TIME;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    *recomp_memory_u32(TEST_TIME) = 0u;
    *recomp_memory_u32(TEST_TIME + 4u) = 0u;
    to_time();
    passed &= expect_u32("to-time result", recomp_runtime.registers.eax, 1u);
    passed &= expect_u32(
        "round-trip low", *recomp_memory_u32(TEST_TIME), (uint32_t)vector->time);
    passed &= expect_u32(
        "round-trip high",
        *recomp_memory_u32(TEST_TIME + 4u),
        (uint32_t)(vector->time >> 32u));
    passed &= expect_u32(
        "to-time ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 12u);

    /* A null destination is the guest's business, not a crash. */
    stack[0] = 0x0010abcdu;
    stack[1] = TEST_TIME;
    stack[2] = 0u;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    to_fields();
    passed &= expect_u32(
        "null destination ESP",
        recomp_runtime.registers.esp,
        TEST_ENTRY_ESP + 12u);

    return passed;
}
