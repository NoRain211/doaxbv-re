#include "apu_model.h"

#include <string.h>

enum {
    /* The guest spins until the fence reaches a per-site threshold, so the
       fence has to keep rising on its own. Thresholds seen in the generated
       readers are 4, 8, 0x20 and 0x80 after masking the low two bits, plus one
       computed as (active voices << 3) compared against the fence shifted
       right by two. A step of 0x100 clears the fixed thresholds in a single
       read and the computed one within a bounded number of reads. */
    APU_FENCE_STEP = 0x100u,
};

void recomp_apu_init(RecompApu *apu)
{
    memset(apu, 0, sizeof(*apu));
}

bool recomp_apu_contains(uint32_t guest_address, size_t width)
{
    uint64_t access_end = (uint64_t)guest_address + width;

    return width != 0u &&
           guest_address >= RECOMP_APU_BASE &&
           access_end <= (uint64_t)RECOMP_APU_BASE + RECOMP_APU_SIZE;
}

uint8_t *recomp_apu_access(
    RecompApu *apu,
    uint32_t guest_address,
    size_t width)
{
    uint32_t offset;

    if (!recomp_apu_contains(guest_address, width)) {
        return NULL;
    }

    offset = guest_address - RECOMP_APU_BASE;
    if (guest_address == RECOMP_APU_FENCE_ADDRESS &&
        width == sizeof(uint32_t)) {
        uint32_t fence;

        memcpy(&fence, apu->registers + offset, sizeof(fence));
        fence += APU_FENCE_STEP;
        memcpy(apu->registers + offset, &fence, sizeof(fence));
    }

    return apu->registers + offset;
}
