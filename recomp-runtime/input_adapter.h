#ifndef DOAXBV_RECOMP_INPUT_ADAPTER_H
#define DOAXBV_RECOMP_INPUT_ADAPTER_H

#include "input_model.h"
#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Returns false when no pad is present on the port. */
typedef bool (*RecompInputSampleSource)(
    uint32_t port,
    RecompInputGamepad *gamepad);

RecompFunction recomp_input_lookup_manual(uint32_t guest_address);
void recomp_input_adapter_reset(void);
void recomp_input_adapter_set_source(RecompInputSampleSource source);
const RecompInputModel *recomp_input_adapter_model(void);

#ifdef __cplusplus
}
#endif

#endif
