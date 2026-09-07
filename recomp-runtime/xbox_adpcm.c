#include "xbox_adpcm.h"

static const int16_t step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544,
    598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707,
    1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
    5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635,
    13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static const int8_t index_adjustment[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

int xbox_adpcm_decode_block(
    const uint8_t *block, size_t block_bytes, unsigned channels,
    int16_t *pcm, size_t pcm_samples)
{
    if (block == NULL || pcm == NULL || channels < 1u || channels > 2u ||
        block_bytes != XBOX_ADPCM_BLOCK_BYTES * channels ||
        pcm_samples < XBOX_ADPCM_BLOCK_SAMPLES * channels) {
        return 0;
    }
    for (unsigned channel = 0u; channel < channels; ++channel) {
        if (block[channel * 4u + 2u] > 88u || block[channel * 4u + 3u] != 0u) {
            return 0;
        }
    }

    for (unsigned channel = 0u; channel < channels; ++channel) {
        const uint8_t *header = block + channel * 4u;
        int predictor = header[0] | (header[1] << 8);
        int index = header[2];

        if (predictor >= 32768) {
            predictor -= 65536;
        }
        pcm[channel] = (int16_t)predictor;
        /* Xbox emits the header predictor and 63 deltas, skipping the final
           high nibble. Stereo payload alternates four bytes per channel. */
        for (unsigned n = 0u; n < XBOX_ADPCM_BLOCK_SAMPLES - 1u; ++n) {
            unsigned offset = 4u * channels + (n / 8u) * 4u * channels +
                              4u * channel + (n % 8u) / 2u;
            unsigned code = (block[offset] >> ((n & 1u) * 4u)) & 15u;
            int step = step_table[index];
            int delta = step >> 3;

            if (code & 1u) delta += step >> 2;
            if (code & 2u) delta += step >> 1;
            if (code & 4u) delta += step;
            predictor += (code & 8u) ? -delta : delta;
            if (predictor > 32767) predictor = 32767;
            if (predictor < -32768) predictor = -32768;
            pcm[(n + 1u) * channels + channel] = (int16_t)predictor;

            index += index_adjustment[code & 7u];
            if (index < 0) index = 0;
            if (index > 88) index = 88;
        }
    }
    return 1;
}
