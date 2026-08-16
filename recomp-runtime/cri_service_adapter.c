#include "cri_service_adapter.h"

#include "stop_report.h"

#include <inttypes.h>
#include <stdio.h>

enum {
    ADXF_GET_STAT_ADDRESS = 0x001877d0u,
    ADXF_OPEN_ADDRESS = 0x00187e40u,
    ADXF_GET_PT_STAT_ADDRESS = 0x00187b30u,
    CRI_SYNC_CALLBACK_ADDRESS = 0x00189cb0u,
    CRI_LANE2_SERVICE_ADDRESS = 0x00193e30u,
    CRI_LANE5_SERVICE_ADDRESS = 0x00193e50u,
    MWP_FRAME_GET_STATUS_ADDRESS = 0x00198320u,
    CRI_SYNC_FLAG_ADDRESS = 0x003b772cu,
    CRI_PARTITION_COUNT_ADDRESS = 0x005e69d0u,
};

#ifdef RECOMP_FULL_PROGRAM
void sub_001877D0(void);
void sub_00187B30(void);
void sub_00187E40(void);
void sub_00189970(void);
void sub_00193E30(void);
void sub_00193E50(void);
void sub_00198320(void);
void sub_001E38F0(void);

static const RecompCriServiceHooks default_hooks = {
    .lane2_service = sub_00193E30,
    .file_worker_io = sub_001E38F0,
    .file_worker_service = sub_00189970,
    .lane5_service = sub_00193E50,
    .adxf_get_stat = sub_001877D0,
    .adxf_get_pt_stat = sub_00187B30,
    .adxf_open = sub_00187E40,
    .mwp_frame_get_status = sub_00198320,
};
#else
static const RecompCriServiceHooks default_hooks;
#endif

typedef struct RecompCriGuestStep {
    const RecompFunction *functions;
    size_t function_count;
    int balanced;
} RecompCriGuestStep;

static RecompCriServiceModel service_model;
static RecompCriServiceHooks service_hooks;
static int reported_lane2;
static int reported_lane5;
static int reported_catalog;
static int reported_adxf_open;
static uint32_t mwp_status_calls;
static uint32_t prior_mwp_status;
static int have_prior_mwp_status;

static void run_guest_step(void *context)
{
    RecompCriGuestStep *step = context;
    step->balanced = 1;
    for (size_t i = 0u; i < step->function_count; ++i) {
        RecompRegisters saved = recomp_runtime.registers;

        recomp_runtime.registers.esp -= 4u;
        *recomp_memory_u32(recomp_runtime.registers.esp) = 0u;
        step->functions[i]();
        if (recomp_runtime.registers.esp != saved.esp) {
            step->balanced = 0;
        }
        recomp_runtime.registers = saved;
        if (!step->balanced) {
            break;
        }
    }
}

static RecompCriServiceResult run_service_batch(
    int lane,
    const RecompFunction *functions,
    size_t function_count)
{
    RecompCriGuestStep step = {
        .functions = functions,
        .function_count = function_count,
        .balanced = 0,
    };
    RecompCriServiceResult result;

    if (functions == NULL || function_count == 0u) {
        return RECOMP_CRI_SERVICE_INVALID;
    }
    for (size_t i = 0u; i < function_count; ++i) {
        if (functions[i] == NULL) {
            return RECOMP_CRI_SERVICE_INVALID;
        }
    }
    result = lane == 2
        ? recomp_cri_service_run_lane2(&service_model, run_guest_step, &step)
        : recomp_cri_service_run_lane5(&service_model, run_guest_step, &step);
    if (result == RECOMP_CRI_SERVICE_OK && !step.balanced) {
        recomp_stop(2, "cri-service:lane-%d-stack", lane);
    }
    return result;
}

static RecompCriServiceResult run_lane2_batch(void)
{
    const RecompFunction functions[] = {
        service_hooks.lane2_service,
        service_hooks.file_worker_service,
    };

    return run_service_batch(
        2, functions, sizeof functions / sizeof functions[0]);
}

static RecompCriServiceResult run_file_worker_step(void)
{
    const RecompFunction functions[] = {
        service_hooks.lane2_service,
        service_hooks.file_worker_io,
        service_hooks.file_worker_service,
    };

    return run_service_batch(
        2, functions, sizeof functions / sizeof functions[0]);
}

static void require_lane2_service(RecompCriServiceResult result)
{
    if (result == RECOMP_CRI_SERVICE_INVALID) {
        recomp_stop(2, "cri-service:lane-2-unavailable");
    }
    if (result == RECOMP_CRI_SERVICE_OK && !reported_lane2) {
        fprintf(
            stderr,
            "recomp cri: cooperative lane-2 batch count=%" PRIu32 "\n",
            service_model.lane2_batches);
        reported_lane2 = 1;
    }
}

static void require_lane5_service(RecompCriServiceResult result)
{
    if (result == RECOMP_CRI_SERVICE_INVALID) {
        recomp_stop(2, "cri-service:lane-5-unavailable");
    }
    if (result == RECOMP_CRI_SERVICE_OK && !reported_lane5) {
        fprintf(
            stderr,
            "recomp cri: cooperative lane-5 batch count=%" PRIu32 "\n",
            service_model.lane5_handoffs);
        reported_lane5 = 1;
    }
}

static void recomp_adxf_get_stat_adapter(void)
{
    RecompCriServiceResult lane2_result = run_lane2_batch();

    if (service_hooks.adxf_get_stat == NULL) {
        recomp_stop(2, "cri-service:lane-2-unavailable");
    }
    require_lane2_service(lane2_result);
    service_hooks.adxf_get_stat();
}

static void recomp_adxf_get_pt_stat_adapter(void)
{
    RecompCriServiceResult lane2_result = run_lane2_batch();

    if (service_hooks.adxf_get_pt_stat == NULL) {
        recomp_stop(2, "cri-service:lane-2-unavailable");
    }
    require_lane2_service(lane2_result);
    service_hooks.adxf_get_pt_stat();
#ifdef RECOMP_FULL_PROGRAM
    if (!reported_catalog && recomp_runtime.registers.eax == 3u &&
        *recomp_memory_u32(CRI_PARTITION_COUNT_ADDRESS) == 0x101u) {
        fprintf(
            stderr,
            "recomp cri: generated ADXF_GetPtStat return=3 count=0x101\n");
        reported_catalog = 1;
        recomp_stop_at_boundary("cri-adxf-catalog:ready");
    }
#endif
}

static void recomp_adxf_open_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    RecompCriServiceResult lane2_result = run_file_worker_step();

    if (service_hooks.adxf_open == NULL) {
        recomp_stop(2, "cri-service:adxf-open-unavailable");
    }
    require_lane2_service(lane2_result);
    service_hooks.adxf_open();
    if (!reported_adxf_open && recomp_runtime.registers.eax != 0u) {
        fprintf(
            stderr,
            "recomp cri: generated ADXF_OpenAfs archive=%" PRIu32
            " member=%" PRIu32 " handle=0x%08" PRIx32 "\n",
            *recomp_memory_u32(entry_esp + 4u),
            *recomp_memory_u32(entry_esp + 8u),
            recomp_runtime.registers.eax);
        reported_adxf_open = 1;
        recomp_stop_at_boundary("cri-adxf-open:first-success");
    }
}

static void recomp_mwp_frame_get_status_adapter(void)
{
    RecompCriServiceResult lane2_result = run_file_worker_step();

    if (service_hooks.mwp_frame_get_status == NULL) {
        recomp_stop(2, "cri-service:mwp-status-unavailable");
    }
    require_lane2_service(lane2_result);
    service_hooks.mwp_frame_get_status();
    ++mwp_status_calls;
    if (!have_prior_mwp_status ||
        recomp_runtime.registers.eax != prior_mwp_status) {
        fprintf(
            stderr,
            "recomp cri: generated MWP_FrameGetStatus call=%" PRIu32
            " status=%" PRIu32 "\n",
            mwp_status_calls,
            recomp_runtime.registers.eax);
        prior_mwp_status = recomp_runtime.registers.eax;
        have_prior_mwp_status = 1;
    }
    if (recomp_runtime.registers.eax == 3u ||
        recomp_runtime.registers.eax == 4u) {
        recomp_stop_at_boundary("cri-movie-status:terminal");
    }
}

static void recomp_cri_sync_callback_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    RecompCriServiceResult result;

    *recomp_memory_u32(CRI_SYNC_FLAG_ADDRESS) = 1u;
    result = run_service_batch(5, &service_hooks.lane5_service, 1u);
    if (result != RECOMP_CRI_SERVICE_OK) {
        recomp_stop(2, "cri-service:lane-5-result:%u", (unsigned)result);
    }
    if (*recomp_memory_u32(CRI_SYNC_FLAG_ADDRESS) != 1u) {
        recomp_stop(2, "cri-service:lane-5-handshake");
    }
    *recomp_memory_u32(CRI_SYNC_FLAG_ADDRESS) = 0u;
    if (!reported_lane5) {
        fprintf(
            stderr,
            "recomp cri: cooperative lane-5 handoff count=%" PRIu32 "\n",
            service_model.lane5_handoffs);
        reported_lane5 = 1;
    }
    recomp_runtime.registers.eax = 1u;
    recomp_runtime.registers.esp = entry_esp + 4u;
}

void recomp_cri_service_adapter_reset(void)
{
    recomp_cri_service_reset(&service_model);
    service_hooks = default_hooks;
    reported_lane2 = 0;
    reported_lane5 = 0;
    reported_catalog = 0;
    reported_adxf_open = 0;
    mwp_status_calls = 0u;
    prior_mwp_status = 0u;
    have_prior_mwp_status = 0;
}

void recomp_cri_service_adapter_set_hooks(
    const RecompCriServiceHooks *hooks)
{
    service_hooks = hooks != NULL
        ? *hooks
        : (RecompCriServiceHooks){0};
}

const RecompCriServiceModel *recomp_cri_service_adapter_model(void)
{
    return &service_model;
}

RecompFunction recomp_cri_service_lookup_manual(uint32_t guest_address)
{
    switch (guest_address) {
    case ADXF_GET_STAT_ADDRESS:
        return recomp_adxf_get_stat_adapter;
    case ADXF_GET_PT_STAT_ADDRESS:
        return recomp_adxf_get_pt_stat_adapter;
    case ADXF_OPEN_ADDRESS:
        return recomp_adxf_open_adapter;
    case CRI_SYNC_CALLBACK_ADDRESS:
        return recomp_cri_sync_callback_adapter;
    case MWP_FRAME_GET_STATUS_ADDRESS:
        return recomp_mwp_frame_get_status_adapter;
    default:
        return NULL;
    }
}
