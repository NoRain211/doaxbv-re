#ifndef DOAXBV_RECOMP_OHCI_MODEL_H
#define DOAXBV_RECOMP_OHCI_MODEL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum RecompOhciRegister {
    RECOMP_OHCI_HC_REVISION,
    RECOMP_OHCI_HC_CONTROL,
    RECOMP_OHCI_HC_COMMAND_STATUS,
    RECOMP_OHCI_HC_INTERRUPT_ENABLE,
    RECOMP_OHCI_HC_HCCA,
    RECOMP_OHCI_HC_PERIOD_CURRENT_ED,
    RECOMP_OHCI_HC_CONTROL_HEAD_ED,
    RECOMP_OHCI_HC_CONTROL_CURRENT_ED,
    RECOMP_OHCI_HC_BULK_HEAD_ED,
    RECOMP_OHCI_HC_BULK_CURRENT_ED,
    RECOMP_OHCI_HC_DONE_HEAD,
    RECOMP_OHCI_HC_FM_INTERVAL,
    RECOMP_OHCI_HC_PERIODIC_START,
    RECOMP_OHCI_HC_LS_THRESHOLD,
    RECOMP_OHCI_HC_RH_DESCRIPTOR_A,
    RECOMP_OHCI_HC_RH_DESCRIPTOR_B,
    RECOMP_OHCI_HC_RH_STATUS,
    RECOMP_OHCI_HC_RH_PORT_STATUS_1,
    RECOMP_OHCI_HC_RH_PORT_STATUS_2,
    RECOMP_OHCI_HC_RH_PORT_STATUS_3,
    RECOMP_OHCI_HC_RH_PORT_STATUS_4,
} RecompOhciRegister;

typedef struct RecompOhci {
    uint32_t hc_control;
    uint32_t hc_command_status;
    uint32_t hc_interrupt_enable;
    uint32_t hc_hcca;
    uint32_t hc_control_head_ed;
    uint32_t hc_control_current_ed;
    uint32_t hc_bulk_head_ed;
    uint32_t hc_bulk_current_ed;
    uint32_t hc_fm_interval;
    uint32_t hc_periodic_start;
    uint32_t hc_ls_threshold;
    uint32_t hc_rh_descriptor_a;
    uint32_t hc_rh_descriptor_b;
    uint32_t hc_rh_status;
} RecompOhci;

void recomp_ohci_init(RecompOhci *ohci);
bool recomp_ohci_read(
    const RecompOhci *ohci,
    RecompOhciRegister reg,
    uint32_t *value);
bool recomp_ohci_write(
    RecompOhci *ohci,
    RecompOhciRegister reg,
    uint32_t value);

#endif
