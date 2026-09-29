#include "crt_format_model.h"

#include <stdio.h>
#include <string.h>

/*
 * Length of the supported directive at percent, or 0 if it is unsupported or
 * cut off by the end of the string. Stores the conversion character.
 */
static size_t scan_directive(const char *percent, char *conversion)
{
    size_t flags = strspn(percent + 1, "-+ 0#");
    size_t length = 1u + flags;
    const char *permitted;

    length += strspn(percent + length, "0123456789");
    switch (percent[length]) {
    case 'd':
    case 'i':
        permitted = "-+ 0";
        break;
    case 'u':
        permitted = "-0";
        break;
    case 'x':
    case 'X':
        permitted = "-0#";
        break;
    case 'c':
    case 's':
        permitted = "-";
        break;
    default:
        return 0u;
    }
    if (strspn(percent + 1, permitted) != flags) {
        return 0u;
    }
    *conversion = percent[length];
    return length + 1u;
}

RecompCrtFormatResult recomp_crt_format(
    char *destination,
    size_t destination_capacity,
    const char *format,
    RecompCrtFormatFetch fetch,
    void *context,
    size_t *written)
{
    size_t total = 0u;
    size_t used = 0u;

    if (destination == NULL || destination_capacity == 0u || format == NULL ||
        fetch == NULL || written == NULL) {
        return RECOMP_CRT_FORMAT_INVALID_ARGUMENT;
    }
    while (*format != '\0') {
        const char *percent = strchr(format, '%');
        size_t literal = percent == NULL ? strlen(format)
                                         : (size_t)(percent - format);
        RecompCrtFormatArgument argument = {0};
        char conversion;
        char spec[32];
        size_t spec_length;
        int length;

        if (literal >= destination_capacity - total) {
            return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
        }
        memcpy(destination + total, format, literal);
        total += literal;
        if (percent == NULL) {
            break;
        }
        if (percent[1] == '%') {
            if (1u >= destination_capacity - total) {
                return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
            }
            destination[total++] = '%';
            format = percent + 2;
            continue;
        }
        spec_length = scan_directive(percent, &conversion);
        if (spec_length == 0u || spec_length >= sizeof spec) {
            return RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE;
        }
        memcpy(spec, percent, spec_length);
        spec[spec_length] = '\0';
        if (!fetch(context, used, conversion, &argument)) {
            return RECOMP_CRT_FORMAT_INVALID_ARGUMENT;
        }
        switch (conversion) {
        case 's':
            if (argument.string == NULL) {
                return RECOMP_CRT_FORMAT_INVALID_ARGUMENT;
            }
            length = snprintf(destination + total,
                destination_capacity - total, spec, argument.string);
            break;
        case 'c':
            length = snprintf(destination + total,
                destination_capacity - total, spec, (int)argument.value);
            break;
        case 'd':
        case 'i':
            length = snprintf(destination + total,
                destination_capacity - total, spec, (int32_t)argument.value);
            break;
        default:
            length = snprintf(destination + total,
                destination_capacity - total, spec, argument.value);
            break;
        }
        if (length < 0) {
            return RECOMP_CRT_FORMAT_INVALID_ARGUMENT;
        }
        if ((size_t)length >= destination_capacity - total) {
            return RECOMP_CRT_FORMAT_OUTPUT_TOO_SMALL;
        }
        total += (size_t)length;
        ++used;
        format = percent + spec_length;
    }
    destination[total] = '\0';
    *written = total;
    return RECOMP_CRT_FORMAT_OK;
}
