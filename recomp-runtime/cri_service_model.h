#ifndef DOAXBV_RECOMP_CRI_SERVICE_MODEL_H
#define DOAXBV_RECOMP_CRI_SERVICE_MODEL_H

#include <stdint.h>

typedef enum RecompCriServiceResult {
    RECOMP_CRI_SERVICE_INVALID = 0,
    RECOMP_CRI_SERVICE_OK = 1,
    RECOMP_CRI_SERVICE_BUSY = 2,
} RecompCriServiceResult;

typedef void (*RecompCriServiceStep)(void *context);

typedef struct RecompCriServiceModel {
    uint32_t lane2_active;
    uint32_t lane5_active;
    uint32_t lane2_batches;
    uint32_t lane2_busy_skips;
    uint32_t lane5_handoffs;
    uint32_t lane5_busy_skips;
} RecompCriServiceModel;

void recomp_cri_service_reset(RecompCriServiceModel *model);
RecompCriServiceResult recomp_cri_service_run_lane2(
    RecompCriServiceModel *model,
    RecompCriServiceStep step,
    void *context);
RecompCriServiceResult recomp_cri_service_run_lane5(
    RecompCriServiceModel *model,
    RecompCriServiceStep step,
    void *context);

#endif
