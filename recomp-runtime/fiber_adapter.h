#ifndef DOAXBV_RECOMP_FIBER_ADAPTER_H
#define DOAXBV_RECOMP_FIBER_ADAPTER_H

#include "fiber_model.h"
#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

RecompFunction recomp_fiber_lookup_manual(uint32_t guest_address);
void recomp_fiber_adapter_reset(void);
const RecompFiberModel *recomp_fiber_adapter_model(void);
void recomp_fiber_adapter_report(void);

#ifdef __cplusplus
}
#endif

#endif
