/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "runtime.h"

void sub_00189800(void)
{
    uint32_t object_address = *recomp_memory_u32(recomp_runtime.registers.esp + 4u);

    recomp_runtime.registers.eax = *recomp_memory_u32(object_address + 0x1cu);
    recomp_runtime.registers.esp += 4u;
}
