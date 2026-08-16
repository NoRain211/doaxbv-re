#include "fiber_model.h"

#include <string.h>

void recomp_fiber_model_reset(RecompFiberModel *model)
{
    if (model != NULL) {
        *model = (RecompFiberModel){0};
    }
}

RecompFiber *recomp_fiber_find(
    RecompFiberModel *model,
    uint32_t guest_handle)
{
    if (model == NULL || guest_handle == 0u) {
        return NULL;
    }
    for (size_t i = 0u; i < RECOMP_FIBER_MAX_COUNT; ++i) {
        RecompFiber *fiber = &model->fibers[i];

        if (fiber->active && fiber->guest_handle == guest_handle) {
            return fiber;
        }
    }
    return NULL;
}

RecompFiber *recomp_fiber_add(
    RecompFiberModel *model,
    uint32_t guest_handle,
    uint32_t entry,
    uint32_t parameter,
    const RecompRegisters *registers,
    uint32_t exception_list)
{
    RecompFiber *fiber;

    if (model == NULL || guest_handle == 0u || registers == NULL ||
        recomp_fiber_find(model, guest_handle) != NULL) {
        return NULL;
    }
    for (size_t i = 0u; i < RECOMP_FIBER_MAX_COUNT; ++i) {
        fiber = &model->fibers[i];
        if (!fiber->active) {
            *fiber = (RecompFiber){
                .active = true,
                .guest_handle = guest_handle,
                .entry = entry,
                .parameter = parameter,
                .exception_list = exception_list,
                .registers = *registers,
            };
            return fiber;
        }
    }
    return NULL;
}

RecompFiber *recomp_fiber_current(RecompFiberModel *model)
{
    return model == NULL
        ? NULL
        : recomp_fiber_find(model, model->current_handle);
}

bool recomp_fiber_set_current(
    RecompFiberModel *model,
    uint32_t guest_handle)
{
    if (recomp_fiber_find(model, guest_handle) == NULL) {
        return false;
    }
    model->current_handle = guest_handle;
    return true;
}

bool recomp_fiber_remove(
    RecompFiberModel *model,
    uint32_t guest_handle)
{
    RecompFiber *fiber = recomp_fiber_find(model, guest_handle);

    if (fiber == NULL || model->current_handle == guest_handle) {
        return false;
    }
    *fiber = (RecompFiber){0};
    return true;
}
