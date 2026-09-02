#ifndef DOAXBV_RECOMP_FIBER_MODEL_H
#define DOAXBV_RECOMP_FIBER_MODEL_H

#include "runtime.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    RECOMP_FIBER_MAX_COUNT = 32u,
};

typedef struct RecompFiber {
    bool active;
    uint32_t guest_handle;
    uint32_t entry;
    uint32_t parameter;
    uint32_t exception_list;
    RecompRegisters registers;
    /* Each hardware thread owns a full x87 and SSE context, and the Xbox
       saves it across a context switch. The runtime keeps that state outside
       RecompRegisters so a value can straddle the several C bodies one guest
       routine is split into, which makes it global - and therefore shared by
       every fiber unless it is saved here. Measured: the x87 stack was
       non-empty at 240 of 240 switches, so live floating point does cross
       fiber boundaries in this program. */
    RecompFpuContext fpu;
} RecompFiber;

typedef struct RecompFiberModel {
    RecompFiber fibers[RECOMP_FIBER_MAX_COUNT];
    uint32_t current_handle;
} RecompFiberModel;

void recomp_fiber_model_reset(RecompFiberModel *model);
RecompFiber *recomp_fiber_add(
    RecompFiberModel *model,
    uint32_t guest_handle,
    uint32_t entry,
    uint32_t parameter,
    const RecompRegisters *registers,
    uint32_t exception_list);
RecompFiber *recomp_fiber_find(
    RecompFiberModel *model,
    uint32_t guest_handle);
RecompFiber *recomp_fiber_current(RecompFiberModel *model);
bool recomp_fiber_set_current(
    RecompFiberModel *model,
    uint32_t guest_handle);
bool recomp_fiber_remove(
    RecompFiberModel *model,
    uint32_t guest_handle);

#endif
