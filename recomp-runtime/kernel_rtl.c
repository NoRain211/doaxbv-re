#include "kernel_abi.h"

#include <stdint.h>

uint32_t recomp_kernel_ntstatus_to_dos_error(uint32_t status)
{
    return status;
}

static void bridge_rtl_ntstatus_to_dos_error(void)
{
    uint32_t status = kernel_arg(1u);

    kernel_return(1u, recomp_kernel_ntstatus_to_dos_error(status));
}

/* RtlInitAnsiString lays a guest ANSI_STRING {Length u16, MaximumLength u16,
   Buffer u32} over an existing NUL-terminated string without copying it. The
   buffer pointer is the guest source address itself. */
static void bridge_rtl_init_ansi_string(void)
{
    uint32_t destination = kernel_arg(1u);
    uint32_t source = kernel_arg(2u);

    uint32_t length = 0u;
    if (source != 0u) {
        while (length < 0xffffu &&
               *recomp_memory_i8(source + length) != 0) {
            ++length;
        }
    }
    if (destination != 0u) {
        uint8_t *dest = (uint8_t *)recomp_memory_u32(destination);
        dest[0] = (uint8_t)(length & 0xffu);
        dest[1] = (uint8_t)((length >> 8u) & 0xffu);
        uint32_t maximum = source == 0u ? 0u : length + 1u;
        dest[2] = (uint8_t)(maximum & 0xffu);
        dest[3] = (uint8_t)((maximum >> 8u) & 0xffu);
        *recomp_memory_u32(destination + 4u) = source;
    }
    kernel_return(2u, 0u);
}

/* RtlEqualString compares two guest ANSI_STRINGs by content. The native host
   stubbed this to pointer equality, but the guest now compares mounted path
   names by value, so compare the bytes, honouring the case-insensitive flag. */
static uint32_t ansi_char_at(uint32_t string, uint32_t index, int fold)
{
    uint32_t buffer = *recomp_memory_u32(string + 4u);
    uint32_t c = (uint8_t)*recomp_memory_i8(buffer + index);
    if (fold && c >= 'a' && c <= 'z') {
        c -= ('a' - 'A');
    }
    return c;
}

static void bridge_rtl_equal_string(void)
{
    uint32_t string1 = kernel_arg(1u);
    uint32_t string2 = kernel_arg(2u);
    uint32_t case_insensitive = kernel_arg(3u);

    int equal = 0;
    if (string1 != 0u && string2 != 0u) {
        uint8_t *s1 = (uint8_t *)recomp_memory_u32(string1);
        uint8_t *s2 = (uint8_t *)recomp_memory_u32(string2);
        uint32_t length1 = (uint32_t)s1[0] | ((uint32_t)s1[1] << 8u);
        uint32_t length2 = (uint32_t)s2[0] | ((uint32_t)s2[1] << 8u);

        if (length1 == length2) {
            equal = 1;
            for (uint32_t i = 0; i < length1; ++i) {
                if (ansi_char_at(string1, i, case_insensitive != 0u) !=
                    ansi_char_at(string2, i, case_insensitive != 0u)) {
                    equal = 0;
                    break;
                }
            }
        }
    }
    kernel_return(3u, (uint32_t)equal);
}

RecompFunction recomp_kernel_rtl(uint32_t ordinal)
{
    switch (ordinal) {
    case 279u: return bridge_rtl_equal_string;
    case 289u: return bridge_rtl_init_ansi_string;
    case 301u: return bridge_rtl_ntstatus_to_dos_error;
    default: return NULL;
    }
}
