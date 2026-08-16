#include "d3d_tile_adapter.h"
#include "d3d_tile_model.h"
#include "program_manual.h"
#include "runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    TEST_STATIC_BASE = 0x001f0000u,
    TEST_STATIC_SIZE = 0x00007000u,
    TEST_CALL_BASE = 0x28000000u,
    TEST_CALL_SIZE = 0x00001000u,
    TEST_DEVICE = 0x001f3120u,
    TEST_TILE_SOURCE = TEST_CALL_BASE + 0x100u,
    TEST_ENTRY_ESP = TEST_CALL_BASE + 0x200u,
    TEST_TILE_ARRAY_OFFSET = 0x2260u,
    TEST_TILE_ENTRY_SIZE = 0x18u,
};

static int expect_u32(const char *field, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "D3D SetTile: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

static int expect_tile(
    const char *name,
    uint32_t address,
    const uint32_t expected[RECOMP_D3D_TILE_WORD_COUNT])
{
    int passed = 1;

    for (uint32_t i = 0u; i < RECOMP_D3D_TILE_WORD_COUNT; ++i) {
        char field[64];

        snprintf(field, sizeof field, "%s word %u", name, i);
        passed &= expect_u32(
            field, *recomp_memory_u32(address + i * 4u), expected[i]);
    }
    return passed;
}

static void prepare_call(
    uint8_t *call_memory,
    uint32_t index,
    uint32_t source_address)
{
    uint32_t *stack = (uint32_t *)(void *)(
        call_memory + TEST_ENTRY_ESP - TEST_CALL_BASE);

    stack[0] = 0u;
    stack[1] = index;
    stack[2] = source_address;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xa5a5a5a5u;
}

int recomp_d3d_tile_model_test(void)
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
    const uint32_t zero_tile[RECOMP_D3D_TILE_WORD_COUNT] = {0};
    const uint32_t clipped_tile[RECOMP_D3D_TILE_WORD_COUNT] = {
        0x00000001u, 0x00001000u, 3u, 4u, 0u, 0u,
    };
    const uint32_t preserved_tile[RECOMP_D3D_TILE_WORD_COUNT] = {
        0x80000001u, 0x00002000u, 7u, 8u, 9u, 10u,
    };
    RecompD3dTile source = {
        .words = {0x00000001u, 0x00001000u, 3u, 4u, 5u, 6u},
    };
    RecompD3dTile destination;
    RecompFunction adapter;
    uint32_t destination_address;
    uint32_t entry_address;
    int passed = 1;

    recomp_d3d_set_tile(&destination, &source);
    for (uint32_t i = 0u; i < RECOMP_D3D_TILE_WORD_COUNT; ++i) {
        passed &= expect_u32(
            "plain model clipped tile", destination.words[i], clipped_tile[i]);
    }
    source.words[0] |= 0x80000000u;
    recomp_d3d_set_tile(&destination, &source);
    passed &= expect_u32("plain model word 4", destination.words[4], 5u);
    passed &= expect_u32("plain model word 5", destination.words[5], 6u);
    source.words[1] = 0u;
    recomp_d3d_set_tile(&destination, &source);
    for (uint32_t i = 0u; i < RECOMP_D3D_TILE_WORD_COUNT; ++i) {
        passed &= expect_u32(
            "plain model empty tile", destination.words[i], 0u);
    }
    recomp_d3d_set_tile(&destination, NULL);
    for (uint32_t i = 0u; i < RECOMP_D3D_TILE_WORD_COUNT; ++i) {
        passed &= expect_u32(
            "plain model null tile", destination.words[i], 0u);
    }

    /* The index and the device global arrive unchecked from the guest. */
    entry_address = 0xdeadbeefu;
    if (!recomp_d3d_tile_entry_address(TEST_DEVICE, 7u, &entry_address)) {
        fprintf(stderr, "D3D SetTile: index 7 was rejected\n");
        passed = 0;
    }
    passed &= expect_u32(
        "index 7 entry address",
        entry_address,
        TEST_DEVICE + TEST_TILE_ARRAY_OFFSET + 7u * TEST_TILE_ENTRY_SIZE);

    entry_address = 0xdeadbeefu;
    if (recomp_d3d_tile_entry_address(TEST_DEVICE, 8u, &entry_address)) {
        fprintf(stderr, "D3D SetTile: index 8 was accepted\n");
        passed = 0;
    }
    passed &= expect_u32(
        "index 8 leaves address untouched", entry_address, 0xdeadbeefu);

    /* 0xFFFFFFFF * 0x18 wraps to TEST_DEVICE + 0x2248, a mapped address. */
    entry_address = 0xdeadbeefu;
    if (recomp_d3d_tile_entry_address(TEST_DEVICE, 0xffffffffu,
            &entry_address)) {
        fprintf(stderr, "D3D SetTile: wrapping index was accepted\n");
        passed = 0;
    }
    passed &= expect_u32(
        "wrapping index leaves address untouched",
        entry_address,
        0xdeadbeefu);

    entry_address = 0xdeadbeefu;
    if (recomp_d3d_tile_entry_address(0u, 0u, &entry_address)) {
        fprintf(stderr, "D3D SetTile: null device was accepted\n");
        passed = 0;
    }
    passed &= expect_u32(
        "null device leaves address untouched", entry_address, 0xdeadbeefu);

    /* A guest-writable device global near the top of the address space
       would carry the array past 32 bits and wrap back into low memory. */
    entry_address = 0xdeadbeefu;
    if (!recomp_d3d_tile_entry_address(
            0xffffffffu - RECOMP_D3D_TILE_ARRAY_EXTENT, 7u, &entry_address)) {
        fprintf(stderr, "D3D SetTile: highest fitting device was rejected\n");
        passed = 0;
    }
    passed &= expect_u32(
        "highest fitting device entry address",
        entry_address,
        (0xffffffffu - RECOMP_D3D_TILE_ARRAY_EXTENT) +
            TEST_TILE_ARRAY_OFFSET + 7u * TEST_TILE_ENTRY_SIZE);

    entry_address = 0xdeadbeefu;
    if (recomp_d3d_tile_entry_address(
            0xffffffffu - RECOMP_D3D_TILE_ARRAY_EXTENT + 1u, 0u,
            &entry_address)) {
        fprintf(stderr, "D3D SetTile: overflowing device base was accepted\n");
        passed = 0;
    }
    passed &= expect_u32(
        "overflowing device base leaves address untouched",
        entry_address,
        0xdeadbeefu);

    entry_address = 0xdeadbeefu;
    if (recomp_d3d_tile_entry_address(0xffffffffu, 0u, &entry_address)) {
        fprintf(stderr, "D3D SetTile: top-of-space device was accepted\n");
        passed = 0;
    }
    passed &= expect_u32(
        "top-of-space device leaves address untouched",
        entry_address,
        0xdeadbeefu);

    /* The source pointer is read at +4 and +20 before anything validates it. */
    if (!recomp_d3d_tile_range_fits(
            0xffffffffu - RECOMP_D3D_TILE_ENTRY_SPAN)) {
        fprintf(stderr, "D3D SetTile: highest fitting source was rejected\n");
        passed = 0;
    }
    if (recomp_d3d_tile_range_fits(
            0xffffffffu - RECOMP_D3D_TILE_ENTRY_SPAN + 1u)) {
        fprintf(stderr, "D3D SetTile: overflowing source was accepted\n");
        passed = 0;
    }
    if (recomp_d3d_tile_range_fits(0xfffffff0u)) {
        fprintf(stderr, "D3D SetTile: wrapping source was accepted\n");
        passed = 0;
    }
    if (!recomp_d3d_tile_range_fits(0u)) {
        fprintf(stderr, "D3D SetTile: zero source was rejected\n");
        passed = 0;
    }

    memset(static_memory, 0xa5, sizeof static_memory);
    memset(call_memory, 0, sizeof call_memory);
    recomp_runtime_init(regions, 2u, NULL, 0u, NULL, 0u);
    *recomp_memory_u32(0x001f2978u) = TEST_DEVICE;
    adapter = recomp_lookup_manual(0x001e4930u);
    if (adapter == NULL) {
        fprintf(stderr, "D3D SetTile: manual lookup did not resolve\n");
        return 0;
    }

    prepare_call(call_memory, 0u, 0u);
    adapter();
    destination_address = TEST_DEVICE + TEST_TILE_ARRAY_OFFSET;
    passed &= expect_tile("null source", destination_address, zero_tile);
    passed &= expect_u32(
        "null source preceding word",
        *recomp_memory_u32(destination_address - 4u),
        0xa5a5a5a5u);
    passed &= expect_u32(
        "null source following word",
        *recomp_memory_u32(destination_address + TEST_TILE_ENTRY_SIZE),
        0xa5a5a5a5u);
    passed &= expect_u32(
        "null source ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 12u);
    passed &= expect_u32(
        "void return EAX", recomp_runtime.registers.eax, 0xa5a5a5a5u);

    memcpy(
        call_memory + TEST_TILE_SOURCE - TEST_CALL_BASE,
        clipped_tile,
        sizeof clipped_tile);
    *recomp_memory_u32(TEST_TILE_SOURCE + 16u) = 0x11111111u;
    *recomp_memory_u32(TEST_TILE_SOURCE + 20u) = 0x22222222u;
    prepare_call(call_memory, 1u, TEST_TILE_SOURCE);
    adapter();
    destination_address += TEST_TILE_ENTRY_SIZE;
    passed &= expect_tile("clipped source", destination_address, clipped_tile);

    memcpy(
        call_memory + TEST_TILE_SOURCE - TEST_CALL_BASE,
        preserved_tile,
        sizeof preserved_tile);
    prepare_call(call_memory, 2u, TEST_TILE_SOURCE);
    adapter();
    destination_address += TEST_TILE_ENTRY_SIZE;
    passed &= expect_tile(
        "high-bit source", destination_address, preserved_tile);

    *recomp_memory_u32(TEST_TILE_SOURCE + 4u) = 0u;
    prepare_call(call_memory, 2u, TEST_TILE_SOURCE);
    adapter();
    passed &= expect_tile("empty source", destination_address, zero_tile);

    /* Highest valid index still lands inside the eight-entry array. */
    memcpy(
        call_memory + TEST_TILE_SOURCE - TEST_CALL_BASE,
        preserved_tile,
        sizeof preserved_tile);
    prepare_call(call_memory, 7u, TEST_TILE_SOURCE);
    adapter();
    passed &= expect_tile(
        "index 7 destination",
        TEST_DEVICE + TEST_TILE_ARRAY_OFFSET + 7u * TEST_TILE_ENTRY_SIZE,
        preserved_tile);
    passed &= expect_u32(
        "index 7 ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 12u);
    passed &= expect_u32(
        "index 7 arg order preserved",
        *recomp_memory_u32(TEST_ENTRY_ESP + 4u),
        7u);

    if (recomp_lookup_manual(0x001e4940u) != NULL) {
        fprintf(stderr, "D3D SetTile: unselected lookup resolved\n");
        passed = 0;
    }
    return passed;
}
