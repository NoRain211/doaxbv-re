#ifndef DOAXBV_RECOMP_PROGRAM_MANUAL_H
#define DOAXBV_RECOMP_PROGRAM_MANUAL_H

#include "runtime.h"

RecompFunction recomp_lookup_manual(uint32_t guest_address);

/* Radio shuffle: a random playlist index other than playing whose location
   mask allows location (a location outside 0..7 allows every entry), or -1
   when there is none. random is any host random value. */
int recomp_music_shuffle_pick(const uint16_t *masks, int count, int playing,
                              int location, uint32_t random);

#endif
