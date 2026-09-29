#ifndef DOAXBV_RECOMP_CRT_FORMAT_MODEL_H
#define DOAXBV_RECOMP_CRT_FORMAT_MODEL_H

#include <stddef.h>
#include <stdint.h>

typedef enum RecompCrtFormatResult {
    RECOMP_CRT_FORMAT_OK = 0,
    RECOMP_CRT_FORMAT_INVALID_ARGUMENT,
    RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL,
    RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE,
} RecompCrtFormatResult;

/*
 * One argument of a guest va_list. value is the raw 32-bit slot; the model
 * reads it for c, d, i, u, x, and X. string is read only for s and must be a
 * NUL-terminated host string that stays valid until the next fetch.
 */
typedef struct RecompCrtFormatArgument {
    uint32_t value;
    const char *string;
} RecompCrtFormatArgument;

/*
 * Supplies argument index (0-based) for a directive with this conversion
 * character. Returns 0 to abort the format.
 */
typedef int (*RecompCrtFormatFetch)(
    void *context,
    size_t index,
    char conversion,
    RecompCrtFormatArgument *argument);

/*
 * Supports literals, %%, and %s %c %d %i %u %x %X with width and the ISO C
 * flags of each conversion (d i: "-+ 0"; u: "-0"; x X: "-0#"; c s: "-").
 * Precision, length modifiers, and other conversions are unsupported.
 * Calls fetch once per directive, right before formatting it.
 */
RecompCrtFormatResult recomp_crt_format(
    char *destination,
    size_t destination_capacity,
    const char *format,
    RecompCrtFormatFetch fetch,
    void *context,
    size_t *written);

#endif
