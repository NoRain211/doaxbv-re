#include "crt_format_model.h"

#include <stdio.h>
#include <string.h>

RecompCrtFormatResult recomp_crt_format_literal(
    char *destination,
    size_t destination_capacity,
    const char *format,
    size_t *written)
{
    size_t length;

    if (destination == NULL || format == NULL || written == NULL) {
        return RECOMP_CRT_FORMAT_INVALID_ARGUMENT;
    }

    length = strlen(format);
    if (length >= destination_capacity) {
        return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
    }
    if (strchr(format, '%') != NULL) {
        return RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE;
    }

    memcpy(destination, format, length + 1u);
    *written = length;
    return RECOMP_CRT_FORMAT_OK;
}

RecompCrtFormatResult recomp_crt_format_one_string(
    char *destination,
    size_t destination_capacity,
    const char *format,
    const char *string_argument,
    size_t *written)
{
    const char *directive;
    size_t argument_length;
    size_t prefix_length;
    size_t remaining;
    size_t suffix_length;

    if (destination == NULL || format == NULL || string_argument == NULL ||
        written == NULL) {
        return RECOMP_CRT_FORMAT_INVALID_ARGUMENT;
    }
    directive = strchr(format, '%');
    if (directive == NULL || directive[1] != 's' ||
        strchr(directive + 2, '%') != NULL) {
        return RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE;
    }

    prefix_length = (size_t)(directive - format);
    argument_length = strlen(string_argument);
    suffix_length = strlen(directive + 2);
    if (prefix_length >= destination_capacity) {
        return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
    }
    remaining = destination_capacity - prefix_length;
    if (argument_length >= remaining) {
        return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
    }
    remaining -= argument_length;
    if (suffix_length >= remaining) {
        return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
    }

    memcpy(destination, format, prefix_length);
    memcpy(destination + prefix_length, string_argument, argument_length);
    memcpy(
        destination + prefix_length + argument_length,
        directive + 2,
        suffix_length + 1u);
    *written = prefix_length + argument_length + suffix_length;
    return RECOMP_CRT_FORMAT_OK;
}

RecompCrtFormatResult recomp_crt_format_two_strings(
    char *destination,
    size_t destination_capacity,
    const char *format,
    const char *first_string_argument,
    const char *second_string_argument,
    size_t *written)
{
    const char *first_directive;
    const char *second_directive;
    size_t first_argument_length;
    size_t middle_length;
    size_t prefix_length;
    size_t remaining;
    size_t second_argument_length;
    size_t suffix_length;
    size_t total_length;

    if (destination == NULL || format == NULL ||
        first_string_argument == NULL || second_string_argument == NULL ||
        written == NULL) {
        return RECOMP_CRT_FORMAT_INVALID_ARGUMENT;
    }
    first_directive = strchr(format, '%');
    if (first_directive == NULL || first_directive[1] != 's') {
        return RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE;
    }
    second_directive = strchr(first_directive + 2, '%');
    if (second_directive == NULL || second_directive[1] != 's' ||
        strchr(second_directive + 2, '%') != NULL) {
        return RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE;
    }

    prefix_length = (size_t)(first_directive - format);
    first_argument_length = strlen(first_string_argument);
    middle_length = (size_t)(second_directive - (first_directive + 2));
    second_argument_length = strlen(second_string_argument);
    suffix_length = strlen(second_directive + 2);
    if (prefix_length >= destination_capacity) {
        return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
    }
    remaining = destination_capacity - prefix_length;
    if (first_argument_length >= remaining) {
        return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
    }
    remaining -= first_argument_length;
    if (middle_length >= remaining) {
        return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
    }
    remaining -= middle_length;
    if (second_argument_length >= remaining) {
        return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
    }
    remaining -= second_argument_length;
    if (suffix_length >= remaining) {
        return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
    }

    memcpy(destination, format, prefix_length);
    total_length = prefix_length;
    memcpy(
        destination + total_length,
        first_string_argument,
        first_argument_length);
    total_length += first_argument_length;
    memcpy(destination + total_length, first_directive + 2, middle_length);
    total_length += middle_length;
    memcpy(
        destination + total_length,
        second_string_argument,
        second_argument_length);
    total_length += second_argument_length;
    memcpy(
        destination + total_length,
        second_directive + 2,
        suffix_length + 1u);
    total_length += suffix_length;
    *written = total_length;
    return RECOMP_CRT_FORMAT_OK;
}

RecompCrtFormatResult recomp_crt_format_one_signed_decimal(
    char *destination,
    size_t destination_capacity,
    const char *format,
    int32_t argument,
    size_t *written)
{
    const char *directive;
    int length;

    if (destination == NULL || format == NULL || written == NULL) {
        return RECOMP_CRT_FORMAT_INVALID_ARGUMENT;
    }
    directive = strchr(format, '%');
    if (directive == NULL || directive[1] != 'd' ||
        strchr(directive + 2, '%') != NULL) {
        return RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE;
    }

    length = snprintf(destination, destination_capacity, format, argument);
    if (length < 0) {
        return RECOMP_CRT_FORMAT_INVALID_ARGUMENT;
    }
    if ((size_t)length >= destination_capacity) {
        return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
    }

    *written = (size_t)length;
    return RECOMP_CRT_FORMAT_OK;
}

RecompCrtFormatResult recomp_crt_format_string_signed_decimal(
    char *destination,
    size_t destination_capacity,
    const char *format,
    const char *string_argument,
    int32_t signed_argument,
    size_t *written)
{
    const char *string_directive;
    const char *decimal_directive;
    int length;

    if (destination == NULL || format == NULL || string_argument == NULL ||
        written == NULL) {
        return RECOMP_CRT_FORMAT_INVALID_ARGUMENT;
    }
    string_directive = strchr(format, '%');
    if (string_directive == NULL || string_directive[1] != 's') {
        return RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE;
    }
    decimal_directive = strchr(string_directive + 2, '%');
    if (decimal_directive == NULL || decimal_directive[1] != 'd' ||
        strchr(decimal_directive + 2, '%') != NULL) {
        return RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE;
    }

    length = snprintf(
        destination,
        destination_capacity,
        format,
        string_argument,
        (int)signed_argument);
    if (length < 0) {
        return RECOMP_CRT_FORMAT_INVALID_ARGUMENT;
    }
    if ((size_t)length >= destination_capacity) {
        return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
    }

    *written = (size_t)length;
    return RECOMP_CRT_FORMAT_OK;
}
