#include "movie_color_model.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "Movie color: line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    int16_t table[RECOMP_MOVIE_COLOR_TABLE_ENTRIES] = {0};
    const uint8_t y[] = {0, 1, 2, 238, 238, 3, 4, 5, 238, 238, 6, 7, 8};
    const uint8_t u[] = {10, 20, 238, 30, 40};
    const uint8_t v[] = {1, 2, 238, 238, 3, 4};
    const RecompMoviePlane planes[3] = {
        {y, sizeof y, 5u}, {u, sizeof u, 3u}, {v, sizeof v, 4u}
    };
    const uint8_t expected[3][12] = {
        {10, 0, 4, 255, 11, 0, 5, 255, 23, 1, 11, 255},
        {13, 1, 8, 255, 14, 2, 10, 255, 26, 3, 15, 255},
        {37, 4, 21, 255, 38, 4, 22, 255, 50, 5, 28, 255}
    };
    uint8_t output[49];

    /* Distinct synthetic contributions expose channel order, both chroma
       subsampling axes, independent strides, and six-bit truncation. */
    for (unsigned value = 0u; value < 256u; ++value) {
        table[value * 4u] = (int16_t)(value * 64u);
        table[value * 4u + 1u] = (int16_t)(value * 32u);
        table[value * 4u + 2u] = (int16_t)(value * 96u);
        table[value * 4u + 3u] = 0x3fc0;
        table[1024u + value * 4u] = (int16_t)(value * 64u);
        table[1024u + value * 4u + 1u] = (int16_t)(-(int)value * 4);
        table[1024u + value * 4u + 2u] = (int16_t)(value * 32u);
        table[2048u + value * 4u] = (int16_t)(value * 32u);
        table[2048u + value * 4u + 1u] = (int16_t)(value * 64u);
        table[2048u + value * 4u + 2u] = (int16_t)(-(int)value * 64);
    }
    memset(output, 0xa5, sizeof output);
    CHECK(recomp_movie_color_convert(planes, 3u, 3u, table,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, 44u, 16u));
    for (unsigned row = 0u; row < 3u; ++row) {
        CHECK(memcmp(output + row * 16u, expected[row], 12u) == 0);
        for (unsigned padding = 12u; padding < 16u; ++padding) {
            CHECK(output[row * 16u + padding] == 0xa5u);
        }
    }
    CHECK(output[48] == 0xa5u);

    /* PADDW wraps modulo 16 bits before PSRAW and PACKUSWB. */
    memset(table, 0, sizeof table);
    table[0] = INT16_MAX;
    table[1024u + u[0] * 4u] = 1;
    table[1] = INT16_MIN;
    table[1024u + u[0] * 4u + 1u] = -1;
    table[2] = INT16_MAX;
    table[1024u + u[0] * 4u + 2u] = INT16_MAX;
    table[2048u + v[0] * 4u + 2u] = 130;
    table[3] = 0x3fc0;
    CHECK(recomp_movie_color_convert(planes, 1u, 1u, table,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, 4u, 4u));
    CHECK(output[0] == 0u && output[1] == 255u && output[2] == 2u && output[3] == 255u);
    table[2] = 63;
    table[1024u + u[0] * 4u + 2u] = 0;
    table[2048u + v[0] * 4u + 2u] = 0;
    CHECK(recomp_movie_color_convert(planes, 1u, 1u, table,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, 4u, 4u));
    CHECK(output[2] == 0u && output[3] == 255u);
    table[2] = 64;
    table[3] = 3200;
    table[1024u + u[0] * 4u + 3u] = 128;
    table[2048u + v[0] * 4u + 3u] = -64;
    CHECK(recomp_movie_color_convert(planes, 1u, 1u, table,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, 4u, 4u));
    CHECK(output[2] == 1u && output[3] == 51u);

    /* Every rejection must precede writes, including a short last plane. */
    memset(output, 0xa5, sizeof output);
    CHECK(!recomp_movie_color_convert(NULL, 3u, 3u, table,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, sizeof output, 16u));
    CHECK(!recomp_movie_color_convert(planes, 3u, 3u, NULL,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, sizeof output, 16u));
    CHECK(!recomp_movie_color_convert(planes, 3u, 3u, table,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES - 1u, output, sizeof output, 16u));
    CHECK(!recomp_movie_color_convert(planes, 3u, 3u, table,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES, NULL, sizeof output, 16u));
    CHECK(!recomp_movie_color_convert(planes, 0u, 3u, table,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, sizeof output, 16u));
    CHECK(!recomp_movie_color_convert(planes, 3u, 0u, table,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, sizeof output, 16u));
    CHECK(!recomp_movie_color_convert(planes, 3u, 3u, table,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, 43u, 16u));
    CHECK(!recomp_movie_color_convert(planes, 3u, 3u, table,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, sizeof output, 11u));
    CHECK(!recomp_movie_color_convert(planes, 1u, 3u, table,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, SIZE_MAX, SIZE_MAX));
    CHECK(!recomp_movie_color_convert(planes, UINT32_MAX, UINT32_MAX, table,
        RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, sizeof output, 16u));
    for (unsigned plane = 0u; plane < 3u; ++plane) {
        RecompMoviePlane invalid[3];
        memcpy(invalid, planes, sizeof invalid);
        invalid[plane].data = NULL;
        CHECK(!recomp_movie_color_convert(invalid, 3u, 3u, table,
            RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, sizeof output, 16u));
        invalid[plane] = planes[plane];
        --invalid[plane].size_bytes;
        CHECK(!recomp_movie_color_convert(invalid, 3u, 3u, table,
            RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, sizeof output, 16u));
        invalid[plane] = planes[plane];
        invalid[plane].stride_bytes = plane == 0u ? 2u : 1u;
        CHECK(!recomp_movie_color_convert(invalid, 3u, 3u, table,
            RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, sizeof output, 16u));
        for (unsigned i = 0u; i < 3u; ++i) invalid[i].size_bytes = SIZE_MAX;
        invalid[plane].stride_bytes = SIZE_MAX;
        CHECK(!recomp_movie_color_convert(invalid, 3u, 5u, table,
            RECOMP_MOVIE_COLOR_TABLE_ENTRIES, output, SIZE_MAX, 16u));
    }
    for (unsigned i = 0u; i < sizeof output; ++i) CHECK(output[i] == 0xa5u);
    puts("Movie color: synthetic table and span checks passed");
    return 0;
}
