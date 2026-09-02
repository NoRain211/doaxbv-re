#include "apu_model.h"

#include <stdio.h>
#include <string.h>

static uint32_t read_u32(RecompApu *apu, uint32_t address)
{
    uint32_t value = 0;
    uint8_t *storage = recomp_apu_access(apu, address, sizeof(value));

    if (storage == NULL) {
        return 0u;
    }
    memcpy(&value, storage, sizeof(value));
    return value;
}

/* Each generated fence reader spins until (fence & ~3) >= threshold. */
static int fence_spin_terminates(RecompApu *apu, uint32_t threshold)
{
    for (unsigned reads = 0u; reads < 64u; ++reads) {
        if ((read_u32(apu, RECOMP_APU_FENCE_ADDRESS) & ~3u) >= threshold) {
            return 1;
        }
    }
    return 0;
}

int recomp_apu_model_test(void)
{
    RecompApu apu;
    int passed = 1;

    recomp_apu_init(&apu);
    if (!recomp_apu_contains(RECOMP_APU_FENCE_ADDRESS, 4u)) {
        fprintf(stderr, "APU model: fence is outside the aperture\n");
        passed = 0;
    }
    if (recomp_apu_contains(RECOMP_APU_BASE - 4u, 4u) ||
        recomp_apu_contains(RECOMP_APU_BASE + RECOMP_APU_SIZE, 4u)) {
        fprintf(stderr, "APU model: aperture bounds are wrong\n");
        passed = 0;
    }
    /* An access straddling the top edge must not be served. */
    if (recomp_apu_contains(RECOMP_APU_BASE + RECOMP_APU_SIZE - 2u, 4u)) {
        fprintf(stderr, "APU model: straddling access was accepted\n");
        passed = 0;
    }
    if (recomp_apu_access(&apu, 0xfed00000u, 4u) != NULL) {
        fprintf(stderr, "APU model: claimed an OHCI address\n");
        passed = 0;
    }

    /* Every threshold observed in the generated readers must terminate. */
    recomp_apu_init(&apu);
    if (!fence_spin_terminates(&apu, 4u) ||
        !fence_spin_terminates(&apu, 8u) ||
        !fence_spin_terminates(&apu, 0x20u) ||
        !fence_spin_terminates(&apu, 0x80u)) {
        fprintf(stderr, "APU model: a fence spin did not terminate\n");
        passed = 0;
    }

    /* Ordinary registers keep what the guest wrote. */
    recomp_apu_init(&apu);
    {
        uint8_t *storage = recomp_apu_access(&apu, 0xfe820280u, 4u);
        uint32_t value = 0x5a5a1234u;

        if (storage == NULL) {
            fprintf(stderr, "APU model: voice select is unbacked\n");
            passed = 0;
        } else {
            memcpy(storage, &value, sizeof(value));
            if (read_u32(&apu, 0xfe820280u) != value) {
                fprintf(stderr, "APU model: register did not retain\n");
                passed = 0;
            }
        }
    }

    if (passed) {
        puts("recomp runtime: APU model passed");
    }
    return passed;
}
