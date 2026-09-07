#include "xbox_adpcm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "Xbox ADPCM: line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    uint8_t block[72] = {0};
    int16_t pcm[130];
    const int16_t ramp[] = {1000, 1001, 1004, 1008, 1015, 1027, 1047, 1088, 1082};
    const int16_t clipped[] = {0, 32767, -28669, -32768, 28668, 32763, 32767};

    /* A signed little-endian predictor is sample zero. At index zero,
       zero nibbles leave it unchanged, even after index decrements. */
    block[0] = 0x18u;
    block[1] = 0xfcu;
    block[35] = 0xf0u;
    pcm[64] = 12345;
    CHECK(xbox_adpcm_decode_block(block, 36u, 1u, pcm, 64u));
    for (unsigned i = 0u; i < 64u; ++i) CHECK(pcm[i] == -1000);
    CHECK(pcm[64] == 12345);

    /* Low nibble first; small odd steps distinguish shift/add rounding
       from a single multiply/divide approximation. */
    memset(block, 0, sizeof(block));
    block[0] = 0xe8u;
    block[1] = 0x03u;
    block[4] = 0x21u;
    block[5] = 0x43u;
    block[6] = 0x65u;
    block[7] = 0x87u;
    CHECK(xbox_adpcm_decode_block(block, 36u, 1u, pcm, 64u));
    CHECK(memcmp(pcm, ramp, sizeof(ramp)) == 0);

    /* Step 16 gives exact deltas 2, 6, ..., 30 for all magnitude codes. */
    for (unsigned code = 0u; code < 16u; ++code) {
        int expected = 2 + 4 * (int)(code & 7u);
        memset(block, 0, sizeof(block));
        block[2] = 8u;
        block[4] = (uint8_t)code;
        CHECK(xbox_adpcm_decode_block(block, 36u, 1u, pcm, 64u));
        CHECK(pcm[1] == ((code & 8u) ? -expected : expected));
    }

    /* Saturated samples become history, and the largest index stays 88. */
    memset(block, 0, sizeof(block));
    block[2] = 88u;
    block[4] = 0xf7u;
    block[5] = 0x7fu;
    CHECK(xbox_adpcm_decode_block(block, 36u, 1u, pcm, 64u));
    CHECK(memcmp(pcm, clipped, sizeof(clipped)) == 0);

    /* Each four-byte stereo lane is distinct across every group. */
    memset(block, 0, sizeof(block));
    block[0] = 100u;
    block[4] = 0x9cu;
    block[5] = 0xffu;
    for (unsigned group = 0u; group < 8u; ++group) {
        memset(block + 8u + group * 8u, 0x11, 4u);
        memset(block + 12u + group * 8u, 0x99, 4u);
    }
    block[67] = 0xf1u;
    block[71] = 0x79u;
    pcm[128] = 12345;
    pcm[129] = -12345;
    CHECK(xbox_adpcm_decode_block(block, 72u, 2u, pcm, 128u));
    for (unsigned i = 0u; i < 64u; ++i) {
        CHECK(pcm[i * 2u] == 100 + (int)i);
        CHECK(pcm[i * 2u + 1u] == -100 - (int)i);
    }
    CHECK(pcm[128] == 12345 && pcm[129] == -12345);

    /* Reject bad arguments and either bad channel header before writing. */
    memset(block, 0, sizeof(block));
    for (unsigned i = 0u; i < 130u; ++i) pcm[i] = 12345;
    CHECK(!xbox_adpcm_decode_block(NULL, 36u, 1u, pcm, 64u));
    CHECK(!xbox_adpcm_decode_block(block, 36u, 1u, NULL, 64u));
    CHECK(!xbox_adpcm_decode_block(block, 0u, 0u, pcm, 128u));
    CHECK(!xbox_adpcm_decode_block(block, 72u, 3u, pcm, 128u));
    CHECK(!xbox_adpcm_decode_block(block, 35u, 1u, pcm, 64u));
    CHECK(!xbox_adpcm_decode_block(block, 37u, 1u, pcm, 64u));
    CHECK(!xbox_adpcm_decode_block(block, 71u, 2u, pcm, 128u));
    CHECK(!xbox_adpcm_decode_block(block, 36u, 1u, pcm, 63u));
    CHECK(!xbox_adpcm_decode_block(block, 72u, 2u, pcm, 127u));
    for (unsigned channel = 0u; channel < 2u; ++channel) {
        block[channel * 4u + 2u] = 89u;
        CHECK(!xbox_adpcm_decode_block(block, 72u, 2u, pcm, 128u));
        block[channel * 4u + 2u] = 255u;
        CHECK(!xbox_adpcm_decode_block(block, 72u, 2u, pcm, 128u));
        block[channel * 4u + 2u] = 0u;
        block[channel * 4u + 3u] = 1u;
        CHECK(!xbox_adpcm_decode_block(block, 72u, 2u, pcm, 128u));
        block[channel * 4u + 3u] = 0u;
    }
    for (unsigned i = 0u; i < 130u; ++i) CHECK(pcm[i] == 12345);
    puts("Xbox ADPCM: synthetic block checks passed");
    return 0;
}
