#ifndef DOAXBV_RECOMP_RUNTIME_H
#define DOAXBV_RECOMP_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct RecompRegisters {
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t esp;
} RecompRegisters;

typedef struct RecompMemoryRegion {
    uint32_t address;
    size_t size;
    uint8_t *data;
} RecompMemoryRegion;

typedef struct RecompMemoryAccess {
    uint32_t address;
    uint32_t width;
} RecompMemoryAccess;

typedef void (*RecompFunction)(void);
typedef RecompFunction (*RecompFunctionLookup)(uint32_t guest_address);

typedef struct RecompFunctionEntry {
    uint32_t address;
    RecompFunction function;
} RecompFunctionEntry;

typedef struct RecompRuntime {
    RecompRegisters registers;
    double fpu_stack[8];
    uint32_t fpu_top;
    /* x87 control word. FNSTCW/FLDCW read and write this; the CRT's
       _control87 tests its mask bits to decide whether to raise. */
    uint16_t fpu_control_word;
    /* Result of the last x87 compare: -1, 0, or 1. The hardware keeps this
       in the status word, and the lifter splits one guest routine into
       several C functions, so a compare and the FNSTSW that reads it can
       land in different bodies. */
    int fpu_compare;
    const RecompMemoryRegion *memory_regions;
    size_t memory_region_count;
    RecompMemoryAccess *accesses;
    size_t access_count;
    size_t access_capacity;
    size_t undeclared_access_count;
    const RecompFunctionEntry *functions;
    size_t function_count;
    RecompFunctionLookup lookup;
} RecompRuntime;

#ifdef __cplusplus
extern "C" {
#endif

extern RecompRuntime recomp_runtime;

void recomp_runtime_init(
    const RecompMemoryRegion *memory_regions,
    size_t memory_region_count,
    RecompMemoryAccess *accesses,
    size_t access_capacity,
    const RecompFunctionEntry *functions,
    size_t function_count);
void recomp_runtime_set_lookup(RecompFunctionLookup lookup);
uint32_t *recomp_memory_u32(uint32_t guest_address);
uint64_t *recomp_memory_u64(uint32_t guest_address);
uint16_t *recomp_memory_u16(uint32_t guest_address);
int8_t *recomp_memory_i8(uint32_t guest_address);
void recomp_guest_memcpy(
    uint32_t destination,
    uint32_t source,
    size_t size);
void recomp_guest_memset(
    uint32_t destination,
    int value,
    size_t size);
uint32_t *recomp_ebp_register(void);
uint32_t *recomp_esp_register(void);
bool recomp_dispatch(uint32_t guest_address);
void recomp_dispatch_indirect(uint32_t guest_address, uint32_t saved_esp);
void recomp_generated_breakpoint(const char *member, int line);
const char *recomp_kernel_ordinal_name(uint32_t ordinal);

extern uint32_t recomp_last_dispatch_address;

#ifdef __cplusplus
}
#endif

#if defined(RECOMP_GENERATED_CODE) && \
    !defined(RECOMP_RUNTIME_NO_GENERATED_MACROS)
#define eax recomp_runtime.registers.eax
#define ecx recomp_runtime.registers.ecx
#define edx recomp_runtime.registers.edx
#define ebx recomp_runtime.registers.ebx
#define esi recomp_runtime.registers.esi
#define edi recomp_runtime.registers.edi
/* ponytail: global EBP preserves register state across split generated blocks. */
#define ebp (*recomp_ebp_register())
#define g_esp (*recomp_esp_register())
#define esp (*recomp_esp_register())
#define g_seh_ebp (*recomp_ebp_register())
#define MEM32(address) (*recomp_memory_u32((uint32_t)(address)))
#define MEM16(address) (*recomp_memory_u16((uint32_t)(address)))
#define MEM8(address) \
    (*(uint8_t *)(void *)recomp_memory_i8((uint32_t)(address)))
#define SMEM16(address) \
    (*(int16_t *)(void *)recomp_memory_u16((uint32_t)(address)))
#define SMEM8(address) (*recomp_memory_i8((uint32_t)(address)))
#define XBOX_PTR(address) ((void *)(uintptr_t)(uint32_t)(address))
#define memcpy(destination, source, size) \
    recomp_guest_memcpy( \
        (uint32_t)(uintptr_t)(destination), \
        (uint32_t)(uintptr_t)(source), \
        (size_t)(size))
#define ZX8(value) ((uint32_t)(uint8_t)(value))
#define ZX16(value) ((uint32_t)(uint16_t)(value))
#define SX8(value) ((uint32_t)(int32_t)(int8_t)(uint8_t)(value))
#define LO8(value) ((uint8_t)((value) & 0xffu))
#define HI8(value) ((uint8_t)(((value) >> 8) & 0xffu))
#define LO16(value) ((uint16_t)((value) & 0xffffu))
#define SET_HI8(value, high) \
    ((value) = ((value) & 0xffff00ffu) | \
               ((uint32_t)(uint8_t)(high) << 8))
#define SET_LO8(value, low) \
    ((value) = ((value) & 0xffffff00u) | (uint32_t)(uint8_t)(low))
#define SET_LO16(value, low) \
    ((value) = ((value) & 0xffff0000u) | ((uint32_t)(uint16_t)(low)))
#define CMP_EQ(a, b) ((uint32_t)(a) == (uint32_t)(b))
#define CMP_NE(a, b) ((uint32_t)(a) != (uint32_t)(b))
#define CMP_L(a, b) ((int32_t)(a) < (int32_t)(b))
#define CMP_LE(a, b) ((int32_t)(a) <= (int32_t)(b))
#define CMP_GE(a, b) ((int32_t)(a) >= (int32_t)(b))
#define CMP_G(a, b) ((int32_t)(a) > (int32_t)(b))
#define CMP_A(a, b) ((uint32_t)(a) > (uint32_t)(b))
#define CMP_AE(a, b) ((uint32_t)(a) >= (uint32_t)(b))
#define CMP_BE(a, b) ((uint32_t)(a) <= (uint32_t)(b))
#define TEST_Z(a, b) (((uint32_t)(a) & (uint32_t)(b)) == 0u)
#define TEST_NZ(a, b) (((uint32_t)(a) & (uint32_t)(b)) != 0u)
#define TEST_S(a, b) ((int32_t)((uint32_t)(a) & (uint32_t)(b)) < 0)
#define PUSH32(sp, value) do { \
    uint32_t push_value = (uint32_t)(value); \
    (sp) -= 4u; \
    MEM32(sp) = push_value; \
} while (0)
#define POP32(sp, value) do { \
    (value) = MEM32(sp); \
    (sp) += 4u; \
} while (0)
#define RECOMP_ICALL_SAFE(address, saved_esp) do { \
    recomp_dispatch_indirect( \
        (uint32_t)(address), (uint32_t)(saved_esp)); \
} while (0)
#define RECOMP_ITAIL(address) do { \
    recomp_dispatch_indirect((uint32_t)(address), g_esp); \
} while (0)
#endif

#endif
