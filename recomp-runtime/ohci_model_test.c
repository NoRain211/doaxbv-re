#include "ohci_model.h"

#include <stdio.h>

int recomp_ohci_model_test(void)
{
    RecompOhci ohci;
    uint32_t value;
    int passed = 1;

    recomp_ohci_init(&ohci);
    if (!recomp_ohci_read(&ohci, RECOMP_OHCI_HC_REVISION, &value) ||
        value != 0x00000010u) {
        fprintf(stderr, "OHCI model: HcRevision mismatch\n");
        passed = 0;
    }
    if (!recomp_ohci_read(&ohci, RECOMP_OHCI_HC_CONTROL, &value) ||
        value != 0u) {
        fprintf(stderr, "OHCI model: initial HcControl mismatch\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_RH_DESCRIPTOR_A, 0x00001200u) ||
        !recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_RH_DESCRIPTOR_A, &value) ||
        value != 0x00000204u) {
        fprintf(stderr, "OHCI model: HcRhDescriptorA write mismatch\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_RH_DESCRIPTOR_B, 0u) ||
        !recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_RH_DESCRIPTOR_B, &value) ||
        value != 0u) {
        fprintf(stderr, "OHCI model: HcRhDescriptorB write mismatch\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_RH_STATUS, 0x80000000u) ||
        !recomp_ohci_read(&ohci, RECOMP_OHCI_HC_RH_STATUS, &value) ||
        value != 0u ||
        !recomp_ohci_write(&ohci, RECOMP_OHCI_HC_RH_STATUS, 0u)) {
        fprintf(stderr, "OHCI model: HcRhStatus write mismatch\n");
        passed = 0;
    }
    if (!recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_COMMAND_STATUS, &value) ||
        value != 0u) {
        fprintf(stderr, "OHCI model: initial HcCommandStatus mismatch\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_COMMAND_STATUS, 0x00000001u)) {
        fprintf(stderr, "OHCI model: rejected HCR write\n");
        passed = 0;
    }
    for (unsigned i = 0u; i < 3u; ++i) {
        if (!recomp_ohci_read(
                &ohci, RECOMP_OHCI_HC_COMMAND_STATUS, &value) ||
            value != 0u) {
            fprintf(stderr, "OHCI model: HCR did not complete\n");
            passed = 0;
        }
    }
    if (!recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_RH_DESCRIPTOR_A, &value) ||
        value != 0x00000204u) {
        fprintf(stderr, "OHCI model: HCR reset the root hub\n");
        passed = 0;
    }
    if (recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_COMMAND_STATUS, 0x00000002u)) {
        fprintf(stderr, "OHCI model: accepted unobserved command bit\n");
        passed = 0;
    }
    if (!recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_INTERRUPT_ENABLE, &value) ||
        value != 0x80000000u) {
        fprintf(stderr, "OHCI model: initial HcInterruptEnable mismatch\n");
        passed = 0;
    }
    if (recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_INTERRUPT_ENABLE, 0x00000033u)) {
        fprintf(stderr, "OHCI model: accepted partial interrupt enable\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_INTERRUPT_ENABLE, 0x80000033u) ||
        !recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_INTERRUPT_ENABLE, &value) ||
        value != 0x80000033u) {
        fprintf(stderr, "OHCI model: first HcInterruptEnable mismatch\n");
        passed = 0;
    }
    if (recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_INTERRUPT_ENABLE, 0x00000041u)) {
        fprintf(stderr, "OHCI model: accepted unobserved interrupt bit\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_INTERRUPT_ENABLE, 0x00000040u) ||
        !recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_INTERRUPT_ENABLE, &value) ||
        value != 0x80000073u) {
        fprintf(stderr, "OHCI model: second HcInterruptEnable mismatch\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_HCCA, 0x03fde000u) ||
        !recomp_ohci_read(&ohci, RECOMP_OHCI_HC_HCCA, &value) ||
        value != 0x03fde000u) {
        fprintf(stderr, "OHCI model: HcHCCA storage mismatch\n");
        passed = 0;
    }
    if (recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_HCCA, 0x03fde004u)) {
        fprintf(stderr, "OHCI model: accepted misaligned HcHCCA\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_PERIOD_CURRENT_ED, 0u) ||
        !recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_PERIOD_CURRENT_ED, &value) ||
        value != 0u) {
        fprintf(stderr, "OHCI model: HcPeriodCurrentED no-op mismatch\n");
        passed = 0;
    }
    if (recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_PERIOD_CURRENT_ED, 0x00000010u)) {
        fprintf(stderr, "OHCI model: accepted HcPeriodCurrentED value\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_CONTROL_HEAD_ED, 0x011029a0u) ||
        !recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_CONTROL_HEAD_ED, &value) ||
        value != 0x011029a0u) {
        fprintf(stderr, "OHCI model: HcControlHeadED storage mismatch\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_CONTROL_CURRENT_ED, 0x011029b0u) ||
        !recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_CONTROL_CURRENT_ED, &value) ||
        value != 0x011029b0u) {
        fprintf(stderr, "OHCI model: HcControlCurrentED storage mismatch\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_BULK_HEAD_ED, 0x011029c0u) ||
        !recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_BULK_HEAD_ED, &value) ||
        value != 0x011029c0u) {
        fprintf(stderr, "OHCI model: HcBulkHeadED storage mismatch\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_BULK_CURRENT_ED, 0x011029d0u) ||
        !recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_BULK_CURRENT_ED, &value) ||
        value != 0x011029d0u) {
        fprintf(stderr, "OHCI model: HcBulkCurrentED storage mismatch\n");
        passed = 0;
    }
    if (recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_CONTROL_HEAD_ED, 0x011029a4u)) {
        fprintf(stderr, "OHCI model: accepted misaligned endpoint descriptor\n");
        passed = 0;
    }
    if (!recomp_ohci_write(&ohci, RECOMP_OHCI_HC_DONE_HEAD, 0u) ||
        !recomp_ohci_read(&ohci, RECOMP_OHCI_HC_DONE_HEAD, &value) ||
        value != 0u) {
        fprintf(stderr, "OHCI model: HcDoneHead no-op mismatch\n");
        passed = 0;
    }
    if (recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_DONE_HEAD, 0x00000010u)) {
        fprintf(stderr, "OHCI model: accepted HcDoneHead value\n");
        passed = 0;
    }
    if (!recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_FM_INTERVAL, &value) ||
        value != 0x00002edfu) {
        fprintf(stderr, "OHCI model: initial HcFmInterval mismatch\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_FM_INTERVAL, 0xa7722ed8u) ||
        !recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_FM_INTERVAL, &value) ||
        value != 0xa7722ed8u) {
        fprintf(stderr, "OHCI model: HcFmInterval transition mismatch\n");
        passed = 0;
    }
    if (recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_FM_INTERVAL, 0xa7722ed9u)) {
        fprintf(stderr, "OHCI model: accepted unobserved HcFmInterval\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_PERIODIC_START, 0x00002a29u) ||
        !recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_PERIODIC_START, &value) ||
        value != 0x00002a29u) {
        fprintf(stderr, "OHCI model: HcPeriodicStart storage mismatch\n");
        passed = 0;
    }
    if (recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_PERIODIC_START, 0x00002a28u)) {
        fprintf(stderr, "OHCI model: accepted unobserved HcPeriodicStart\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_LS_THRESHOLD, 0x00000620u) ||
        !recomp_ohci_read(
            &ohci, RECOMP_OHCI_HC_LS_THRESHOLD, &value) ||
        value != 0x00000620u) {
        fprintf(stderr, "OHCI model: HcLSThreshold storage mismatch\n");
        passed = 0;
    }
    if (recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_LS_THRESHOLD, 0x00000621u)) {
        fprintf(stderr, "OHCI model: accepted unobserved HcLSThreshold\n");
        passed = 0;
    }
    if (!recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_CONTROL, 0x000000beu) ||
        !recomp_ohci_read(&ohci, RECOMP_OHCI_HC_CONTROL, &value) ||
        value != 0x000000beu) {
        fprintf(stderr, "OHCI model: HcControl transition mismatch\n");
        passed = 0;
    }
    if (recomp_ohci_write(
            &ohci, RECOMP_OHCI_HC_CONTROL, 0x000000bfu)) {
        fprintf(stderr, "OHCI model: accepted unobserved HcControl bit\n");
        passed = 0;
    }
    for (RecompOhciRegister reg = RECOMP_OHCI_HC_RH_PORT_STATUS_1;
         reg <= RECOMP_OHCI_HC_RH_PORT_STATUS_4;
         reg = (RecompOhciRegister)(reg + 1)) {
        if (!recomp_ohci_read(&ohci, reg, &value) ||
            value != 0x00000100u ||
            !recomp_ohci_write(&ohci, reg, 0u) ||
            !recomp_ohci_read(&ohci, reg, &value) ||
            value != 0x00000100u) {
            fprintf(stderr, "OHCI model: root-port reset state mismatch\n");
            passed = 0;
        }
        if (recomp_ohci_write(&ohci, reg, 0x00010000u)) {
            fprintf(stderr, "OHCI model: accepted connected-port write\n");
            passed = 0;
        }
    }
    if (recomp_ohci_write(&ohci, RECOMP_OHCI_HC_REVISION, 0u)) {
        fprintf(stderr, "OHCI model: accepted HcRevision write\n");
        passed = 0;
    }

    return passed;
}
