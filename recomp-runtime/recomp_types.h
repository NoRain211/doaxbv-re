#ifndef DOAXBV_RECOMP_TYPES_H
#define DOAXBV_RECOMP_TYPES_H

#define RECOMP_RUNTIME_NO_GENERATED_MACROS
#include "runtime.h"
#undef RECOMP_RUNTIME_NO_GENERATED_MACROS

#include <math.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

/* Generated INT3 sites report their guest site instead of trapping the host. */
#undef __debugbreak
#define __debugbreak() recomp_generated_breakpoint(__FILE__, __LINE__)

typedef RecompFunction recomp_func_t;

recomp_func_t recomp_lookup(uint32_t guest_address);
recomp_func_t recomp_lookup_manual(uint32_t guest_address);
recomp_func_t recomp_lookup_kernel(uint32_t guest_address);

extern uint32_t g_recomp_178bb0_origin;
extern uint32_t g_recomp_186fb5_origin;
extern uint32_t g_recomp_17f8e0_origin;
extern uint32_t g_recomp_78d80_origin;
extern uint32_t g_recomp_7c450_origin;
extern uint32_t g_recomp_7c7c0_origin;

#define eax recomp_runtime.registers.eax
#define ecx recomp_runtime.registers.ecx
#define edx recomp_runtime.registers.edx
#define ebx recomp_runtime.registers.ebx
#define esi recomp_runtime.registers.esi
#define edi recomp_runtime.registers.edi
#define esp recomp_runtime.registers.esp
#define g_esp (*recomp_esp_register())
#define g_seh_ebp recomp_runtime.registers.ebp
#define g_fp_stack recomp_runtime.fpu_stack
#define g_fp_top recomp_runtime.fpu_top
#define g_fp_control_word recomp_runtime.fpu_control_word
#define g_fp_cmp recomp_runtime.fpu_compare

#define MEM32(address) (*recomp_memory_u32((uint32_t)(address)))
#define MEM16(address) (*recomp_memory_u16((uint32_t)(address)))
#define MEM8(address) \
    (*(uint8_t *)(void *)recomp_memory_i8((uint32_t)(address)))
#define SMEM32(address) \
    (*(int32_t *)(void *)recomp_memory_u32((uint32_t)(address)))
#define SMEM64(address) \
    (*(int64_t *)(void *)recomp_memory_u64((uint32_t)(address)))
#define SMEM16(address) \
    (*(int16_t *)(void *)recomp_memory_u16((uint32_t)(address)))
#define SMEM8(address) (*recomp_memory_i8((uint32_t)(address)))
#define MEMF(address) \
    (*(float *)(void *)recomp_memory_u32((uint32_t)(address)))
#define MEMD(address) \
    (*(double *)(void *)recomp_memory_u64((uint32_t)(address)))
#define XBOX_PTR(address) ((void *)(uintptr_t)(uint32_t)(address))
#define memcpy(destination, source, size) \
    recomp_guest_memcpy( \
        (uint32_t)(uintptr_t)(destination), \
        (uint32_t)(uintptr_t)(source), \
        (size_t)(size))
#define memset(destination, value, size) \
    recomp_guest_memset( \
        (uint32_t)(uintptr_t)(destination), \
        (int)(value), \
        (size_t)(size))

#define ZX8(value) ((uint32_t)(uint8_t)(value))
#define ZX16(value) ((uint32_t)(uint16_t)(value))
#define SX8(value) ((uint32_t)(int32_t)(int8_t)(uint8_t)(value))
#define SX16(value) ((uint32_t)(int32_t)(int16_t)(uint16_t)(value))
#define LO8(value) ((uint8_t)((value) & 0xffu))
#define HI8(value) ((uint8_t)(((value) >> 8) & 0xffu))
/* x86 PF: set when the low byte of the result has an even number of set
   bits. The CRT branches on it after FNSTSW AX; TEST AH, mask. */
#define PARITY8(value) \
    ((((0x9669u >> (((uint8_t)(value) ^ ((uint8_t)(value) >> 4)) & 0xfu)) \
       & 1u)) != 0u)
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
#define CMP_B(a, b) ((uint32_t)(a) < (uint32_t)(b))
#define CMP_AE(a, b) ((uint32_t)(a) >= (uint32_t)(b))
#define CMP_BE(a, b) ((uint32_t)(a) <= (uint32_t)(b))
#define CMP_A(a, b) ((uint32_t)(a) > (uint32_t)(b))
/* x86 takes SF from the top bit of the operand width, not from bit 31. The
   generated code hands LO8/HI8/LO16 sub-register reads straight to these
   macros, so recover the operand width and sign-extend at that width. A
   sub-register read zero-extends, so casting it to int32_t would make every
   signed test read as non-negative. */
#define RECOMP_FLAG_WIDTH(a, b) (sizeof(a) < sizeof(b) ? sizeof(a) : sizeof(b))
#define RECOMP_SIGNED(value, width) \
    ((width) == 1u ? (int32_t)(int8_t)(uint8_t)(uint32_t)(value) \
     : (width) == 2u ? (int32_t)(int16_t)(uint16_t)(uint32_t)(value) \
     : (int32_t)(uint32_t)(value))
#define CMP_L(a, b) \
    (RECOMP_SIGNED(a, RECOMP_FLAG_WIDTH(a, b)) < \
     RECOMP_SIGNED(b, RECOMP_FLAG_WIDTH(a, b)))
#define CMP_GE(a, b) \
    (RECOMP_SIGNED(a, RECOMP_FLAG_WIDTH(a, b)) >= \
     RECOMP_SIGNED(b, RECOMP_FLAG_WIDTH(a, b)))
#define CMP_LE(a, b) \
    (RECOMP_SIGNED(a, RECOMP_FLAG_WIDTH(a, b)) <= \
     RECOMP_SIGNED(b, RECOMP_FLAG_WIDTH(a, b)))
#define CMP_G(a, b) \
    (RECOMP_SIGNED(a, RECOMP_FLAG_WIDTH(a, b)) > \
     RECOMP_SIGNED(b, RECOMP_FLAG_WIDTH(a, b)))
#define TEST_Z(a, b) (((uint32_t)(a) & (uint32_t)(b)) == 0u)
#define TEST_NZ(a, b) (((uint32_t)(a) & (uint32_t)(b)) != 0u)
#define TEST_S(a, b) \
    (RECOMP_SIGNED((uint32_t)(a) & (uint32_t)(b), \
                   RECOMP_FLAG_WIDTH(a, b)) < 0)

static inline uint32_t ROL32(uint32_t value, unsigned int count)
{
    count &= 31u;
    return count == 0u ? value : (value << count) | (value >> (32u - count));
}

static inline uint32_t ROR32(uint32_t value, unsigned int count)
{
    count &= 31u;
    return count == 0u ? value : (value >> count) | (value << (32u - count));
}

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
    recomp_dispatch_indirect((uint32_t)(address), (uint32_t)(saved_esp)); \
} while (0)
#define RECOMP_ITAIL(address) do { \
    recomp_dispatch_indirect((uint32_t)(address), g_esp); \
} while (0)

#endif
