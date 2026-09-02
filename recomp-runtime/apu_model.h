#ifndef DOAXBV_RECOMP_APU_MODEL_H
#define DOAXBV_RECOMP_APU_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* MCPX audio processor aperture, as the guest addresses it. This is device
   space, not guest RAM, so it is backed here rather than by a memory region. */
#define RECOMP_APU_BASE 0xfe800000u
#define RECOMP_APU_SIZE 0x00060000u

/* Voice-processor fence. Every generated reader spins on it, and the exit
   thresholds differ per site. */
#define RECOMP_APU_FENCE_ADDRESS 0xfe820010u

typedef struct RecompApu {
    uint8_t registers[RECOMP_APU_SIZE];
} RecompApu;

void recomp_apu_init(RecompApu *apu);
bool recomp_apu_contains(uint32_t guest_address, size_t width);

/* Backing storage for one device access, or NULL when the range is outside
   the aperture. Reading the fence advances it. */
uint8_t *recomp_apu_access(
    RecompApu *apu,
    uint32_t guest_address,
    size_t width);

#endif
