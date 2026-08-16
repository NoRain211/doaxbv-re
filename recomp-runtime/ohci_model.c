#include "ohci_model.h"

enum {
    OHCI_CONTROL_INITIALIZATION = 0x000000beu,
    OHCI_COMMAND_HOST_CONTROLLER_RESET = 0x00000001u,
    OHCI_INTERRUPT_ENABLE_INITIAL = 0x80000000u,
    OHCI_INTERRUPT_ENABLE_INITIALIZATION = 0x80000033u,
    OHCI_INTERRUPT_ENABLE_RESET_PATH = 0x00000040u,
    OHCI_FM_INTERVAL_RESET = 0x00002edfu,
    OHCI_OBSERVED_FM_INTERVAL = 0xa7722ed8u,
    OHCI_OBSERVED_PERIODIC_START = 0x00002a29u,
    OHCI_OBSERVED_LS_THRESHOLD = 0x00000620u,
    OHCI_DISCONNECTED_PORT_STATUS = 0x00000100u,
};

static void complete_host_controller_reset(RecompOhci *ohci)
{
    ohci->hc_command_status &= ~OHCI_COMMAND_HOST_CONTROLLER_RESET;
}

static bool store_endpoint_descriptor(uint32_t *reg, uint32_t value)
{
    if ((value & 0x0fu) != 0u) {
        return false;
    }

    *reg = value;
    return true;
}

void recomp_ohci_init(RecompOhci *ohci)
{
    *ohci = (RecompOhci){
        .hc_interrupt_enable = OHCI_INTERRUPT_ENABLE_INITIAL,
        .hc_fm_interval = OHCI_FM_INTERVAL_RESET,
        .hc_rh_descriptor_a = 0x00000004u,
    };
}

bool recomp_ohci_read(
    const RecompOhci *ohci,
    RecompOhciRegister reg,
    uint32_t *value)
{
    switch (reg) {
    case RECOMP_OHCI_HC_REVISION:
        *value = 0x00000010u;
        return true;
    case RECOMP_OHCI_HC_CONTROL:
        *value = ohci->hc_control;
        return true;
    case RECOMP_OHCI_HC_COMMAND_STATUS:
        *value = ohci->hc_command_status;
        return true;
    case RECOMP_OHCI_HC_INTERRUPT_ENABLE:
        *value = ohci->hc_interrupt_enable;
        return true;
    case RECOMP_OHCI_HC_HCCA:
        *value = ohci->hc_hcca;
        return true;
    case RECOMP_OHCI_HC_PERIOD_CURRENT_ED:
        *value = 0u;
        return true;
    case RECOMP_OHCI_HC_CONTROL_HEAD_ED:
        *value = ohci->hc_control_head_ed;
        return true;
    case RECOMP_OHCI_HC_CONTROL_CURRENT_ED:
        *value = ohci->hc_control_current_ed;
        return true;
    case RECOMP_OHCI_HC_BULK_HEAD_ED:
        *value = ohci->hc_bulk_head_ed;
        return true;
    case RECOMP_OHCI_HC_BULK_CURRENT_ED:
        *value = ohci->hc_bulk_current_ed;
        return true;
    case RECOMP_OHCI_HC_DONE_HEAD:
        *value = 0u;
        return true;
    case RECOMP_OHCI_HC_FM_INTERVAL:
        *value = ohci->hc_fm_interval;
        return true;
    case RECOMP_OHCI_HC_PERIODIC_START:
        *value = ohci->hc_periodic_start;
        return true;
    case RECOMP_OHCI_HC_LS_THRESHOLD:
        *value = ohci->hc_ls_threshold;
        return true;
    case RECOMP_OHCI_HC_RH_DESCRIPTOR_A:
        *value = ohci->hc_rh_descriptor_a;
        return true;
    case RECOMP_OHCI_HC_RH_DESCRIPTOR_B:
        *value = ohci->hc_rh_descriptor_b;
        return true;
    case RECOMP_OHCI_HC_RH_STATUS:
        *value = ohci->hc_rh_status;
        return true;
    case RECOMP_OHCI_HC_RH_PORT_STATUS_1:
    case RECOMP_OHCI_HC_RH_PORT_STATUS_2:
    case RECOMP_OHCI_HC_RH_PORT_STATUS_3:
    case RECOMP_OHCI_HC_RH_PORT_STATUS_4:
        *value = OHCI_DISCONNECTED_PORT_STATUS;
        return true;
    }

    return false;
}

bool recomp_ohci_write(
    RecompOhci *ohci,
    RecompOhciRegister reg,
    uint32_t value)
{
    switch (reg) {
    case RECOMP_OHCI_HC_CONTROL:
        if (ohci->hc_control == 0u &&
            value == OHCI_CONTROL_INITIALIZATION) {
            ohci->hc_control = value;
            return true;
        }
        return false;
    case RECOMP_OHCI_HC_COMMAND_STATUS:
        if (value == OHCI_COMMAND_HOST_CONTROLLER_RESET) {
            ohci->hc_command_status |=
                OHCI_COMMAND_HOST_CONTROLLER_RESET;
            /* xemu completes HCR within the MMIO write. */
            complete_host_controller_reset(ohci);
            return true;
        }
        return false;
    case RECOMP_OHCI_HC_INTERRUPT_ENABLE:
        if (ohci->hc_interrupt_enable == OHCI_INTERRUPT_ENABLE_INITIAL &&
            value == OHCI_INTERRUPT_ENABLE_INITIALIZATION) {
            ohci->hc_interrupt_enable |= value;
            return true;
        }
        if (ohci->hc_interrupt_enable ==
                OHCI_INTERRUPT_ENABLE_INITIALIZATION &&
            value == OHCI_INTERRUPT_ENABLE_RESET_PATH) {
            ohci->hc_interrupt_enable |= value;
            return true;
        }
        return false;
    case RECOMP_OHCI_HC_HCCA:
        if ((value & 0xffu) == 0u) {
            ohci->hc_hcca = value;
            return true;
        }
        return false;
    case RECOMP_OHCI_HC_PERIOD_CURRENT_ED:
    case RECOMP_OHCI_HC_DONE_HEAD:
        return value == 0u;
    case RECOMP_OHCI_HC_CONTROL_HEAD_ED:
        return store_endpoint_descriptor(&ohci->hc_control_head_ed, value);
    case RECOMP_OHCI_HC_CONTROL_CURRENT_ED:
        return store_endpoint_descriptor(&ohci->hc_control_current_ed, value);
    case RECOMP_OHCI_HC_BULK_HEAD_ED:
        return store_endpoint_descriptor(&ohci->hc_bulk_head_ed, value);
    case RECOMP_OHCI_HC_BULK_CURRENT_ED:
        return store_endpoint_descriptor(&ohci->hc_bulk_current_ed, value);
    case RECOMP_OHCI_HC_FM_INTERVAL:
        if (ohci->hc_fm_interval == OHCI_FM_INTERVAL_RESET &&
            value == OHCI_OBSERVED_FM_INTERVAL) {
            ohci->hc_fm_interval = value;
            return true;
        }
        return false;
    case RECOMP_OHCI_HC_PERIODIC_START:
        if (value == OHCI_OBSERVED_PERIODIC_START) {
            ohci->hc_periodic_start = value;
            return true;
        }
        return false;
    case RECOMP_OHCI_HC_LS_THRESHOLD:
        if (value == OHCI_OBSERVED_LS_THRESHOLD) {
            ohci->hc_ls_threshold = value;
            return true;
        }
        return false;
    case RECOMP_OHCI_HC_RH_DESCRIPTOR_A:
        if (value == 0x00001200u) {
            ohci->hc_rh_descriptor_a = 0x00000204u;
            return true;
        }
        return false;
    case RECOMP_OHCI_HC_RH_DESCRIPTOR_B:
        if (value == 0u) {
            ohci->hc_rh_descriptor_b = 0u;
            return true;
        }
        return false;
    case RECOMP_OHCI_HC_RH_STATUS:
        if (value == 0x80000000u) {
            ohci->hc_rh_status = 0u;
            return true;
        }
        return value == 0u;
    case RECOMP_OHCI_HC_RH_PORT_STATUS_1:
    case RECOMP_OHCI_HC_RH_PORT_STATUS_2:
    case RECOMP_OHCI_HC_RH_PORT_STATUS_3:
    case RECOMP_OHCI_HC_RH_PORT_STATUS_4:
        return value == 0u;
    case RECOMP_OHCI_HC_REVISION:
        return false;
    }

    return false;
}
