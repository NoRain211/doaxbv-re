#include "program_manual.h"

#include <stdio.h>
#include <string.h>

int recomp_crt_string_adapter_test(void)
{
    enum { BASE = 0x2c000000u, STACK = BASE + 0x100u };
    static uint8_t memory[0x200];
    const RecompMemoryRegion region = {
        .address = BASE, .size = sizeof memory, .data = memory,
    };
    static const struct {
        uint16_t left[4], right[4];
        int32_t expected;
    } cases[] = {
        {{'S', '1', 0}, {'S', '1', 0}, 0},
        {{0}, {0}, 0},
        {{'S', '1', 0}, {'S', '2', 0}, -1},
        {{'S', '2', 0}, {'S', '1', 0}, 1},
        {{'S', 0}, {'S', '1', 0}, -1},
        {{'S', '1', 0}, {'S', 0}, 1},
        {{0x8000, 0}, {0x7fff, 0}, 1},
        {{0x7fff, 0}, {0x8000, 0}, -1},
    };
    RecompFunction compare = recomp_lookup_manual(0x001bb539u);
    if (compare == NULL) {
        fprintf(stderr, "crt string: wcscmp adapter missing\n");
        return 0;
    }
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        recomp_runtime_init(&region, 1u, NULL, 0u, NULL, 0u);
        memcpy(memory, cases[i].left, sizeof cases[i].left);
        memcpy(memory + 0x20u, cases[i].right, sizeof cases[i].right);
        *recomp_memory_u32(STACK) = 0x0010abcdu;
        *recomp_memory_u32(STACK + 4u) = BASE;
        *recomp_memory_u32(STACK + 8u) = BASE + 0x20u;
        recomp_runtime.registers.esp = STACK;
        recomp_runtime.registers.ebx = 0x1234u;
        recomp_runtime.registers.ebp = 0x5678u;
        recomp_runtime.registers.esi = 0x9abcu;
        recomp_runtime.registers.edi = 0xdef0u;
        compare();
        if ((int32_t)recomp_runtime.registers.eax != cases[i].expected ||
            recomp_runtime.registers.esp != STACK + 4u ||
            recomp_runtime.registers.ebx != 0x1234u ||
            recomp_runtime.registers.ebp != 0x5678u ||
            recomp_runtime.registers.esi != 0x9abcu ||
            recomp_runtime.registers.edi != 0xdef0u ||
            *recomp_memory_u32(STACK) != 0x0010abcdu ||
            memcmp(memory, cases[i].left, sizeof cases[i].left) != 0 ||
            memcmp(memory + 0x20u, cases[i].right, sizeof cases[i].right) != 0) {
            fprintf(stderr, "crt string: wcscmp case %zu failed\n", i);
            return 0;
        }
    }
    return 1;
}
