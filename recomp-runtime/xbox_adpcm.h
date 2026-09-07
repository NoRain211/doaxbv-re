#ifndef DOAXBV_RECOMP_XBOX_ADPCM_H
#define DOAXBV_RECOMP_XBOX_ADPCM_H

#include <stddef.h>
#include <stdint.h>

#define XBOX_ADPCM_BLOCK_BYTES 36u
#define XBOX_ADPCM_BLOCK_SAMPLES 64u

#ifdef __cplusplus
extern "C" {
#endif

/* Decode one mono/stereo block to interleaved signed 16-bit PCM.
   Sizes above are per channel; pcm_samples is the output capacity in samples.
   Input and output must not overlap. Returns 1 on success, or 0 without
   changing output for invalid pointers, sizes, channels, or block headers. */
int xbox_adpcm_decode_block(
    const uint8_t *block, size_t block_bytes, unsigned channels,
    int16_t *pcm, size_t pcm_samples);

#ifdef __cplusplus
}
#endif

#endif
