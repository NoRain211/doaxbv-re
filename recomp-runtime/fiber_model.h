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
