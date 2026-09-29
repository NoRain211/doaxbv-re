#include "crt_string_adapter.h"
#include "kernel_abi.h"

static void compare_wide_strings(void)
{
    uint32_t left = kernel_arg(1u);
    uint32_t right = kernel_arg(2u);

    /* Guest wchar_t is unsigned 16-bit, independent of the host ABI. */
    for (;;) {
        uint16_t a = *recomp_memory_u16(left);
        uint16_t b = *recomp_memory_u16(right);
        if (a != b || a == 0u) {
            kernel_return_caller_cleanup((uint32_t)((a > b) - (a < b)));
            return;
        }
        left += 2u;
        right += 2u;
    }
}

RecompFunction recomp_crt_string_lookup_manual(uint32_t guest_address)
{
    return guest_address == 0x001bb539u ? compare_wide_strings : NULL;
}
