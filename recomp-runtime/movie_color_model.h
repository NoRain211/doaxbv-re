#ifndef DOAXBV_RECOMP_MOVIE_COLOR_MODEL_H
#define DOAXBV_RECOMP_MOVIE_COLOR_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { RECOMP_MOVIE_COLOR_TABLE_ENTRIES = 3072u };

typedef struct RecompMoviePlane {
    const uint8_t *data;
    size_t size_bytes;
    size_t stride_bytes;
} RecompMoviePlane;

/* Planes are Y, U, V; chroma dimensions round up for odd image dimensions.
   The table contains 256 four-channel signed contributions for each plane;
   their sum wraps to signed 16 bits before shifting six and clamping to bytes.
   Destination must not overlap the planes or table. Returns false without
   writing for invalid pointers, dimensions, strides, or capacities. */
bool recomp_movie_color_convert(
    const RecompMoviePlane planes[3], uint32_t width, uint32_t height,
    const int16_t *color_table, size_t table_entries,
    uint8_t *destination, size_t destination_bytes, size_t destination_pitch);

#ifdef __cplusplus
}
#endif

#endif
