#ifndef DOAXBV_RECOMP_XAPI_TIME_ADAPTER_H
#define DOAXBV_RECOMP_XAPI_TIME_ADAPTER_H

#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

uint64_t recomp_xapi_performance_counter(void);
uint64_t recomp_xapi_performance_frequency(void);
RecompFunction recomp_xapi_time_lookup_manual(uint32_t guest_address);

#ifdef __cplusplus
}
#endif

#endif
