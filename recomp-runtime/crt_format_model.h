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

RecompCrtFormatResult recomp_crt_format_literal(
    char *destination,
    size_t destination_capacity,
    const char *format,
    size_t *written);

RecompCrtFormatResult recomp_crt_format_one_string(
    char *destination,
    size_t destination_capacity,
    const char *format,
    const char *string_argument,
    size_t *written);

RecompCrtFormatResult recomp_crt_format_two_strings(
    char *destination,
    size_t destination_capacity,
    const char *format,
    const char *first_string_argument,
    const char *second_string_argument,
    size_t *written);

RecompCrtFormatResult recomp_crt_format_one_signed_decimal(
    char *destination,
    size_t destination_capacity,
    const char *format,
    int32_t argument,
    size_t *written);

RecompCrtFormatResult recomp_crt_format_string_signed_decimal(
    char *destination,
    size_t destination_capacity,
    const char *format,
    const char *string_argument,
    int32_t signed_argument,
    size_t *written);

#endif
