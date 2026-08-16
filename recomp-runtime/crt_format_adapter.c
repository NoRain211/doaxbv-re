#include "crt_format_adapter.h"

#include "crt_format_model.h"
#include "stop_report.h"

#include <stdio.h>
#include <string.h>

enum {
    CRT_VSPRINTF_ADDRESS = 0x001ba67cu,
    CRT_FORMAT_LIMIT = 4096u,
};

static uint32_t stack_argument(uint32_t entry_esp, uint32_t index)
{
    return *recomp_memory_u32(entry_esp + 4u + index * 4u);
}

static int read_guest_string(
    uint32_t address,
    char *destination,
    size_t destination_capacity)
{
    for (size_t i = 0u; i < destination_capacity; ++i) {
        destination[i] = (char)*recomp_memory_i8(address + (uint32_t)i);
        if (destination[i] == '\0') {
            return 1;
        }
    }
    return 0;
}

static void recomp_crt_vsprintf_adapter(void)
{
    static int reported_literal_path;
    static int reported_signed_decimal_path;
    static int reported_string_path;
    static int reported_two_string_path;
    static int reported_string_decimal_path;
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t destination_address = stack_argument(entry_esp, 0u);
    uint32_t format_address = stack_argument(entry_esp, 1u);
    uint32_t argument_list_address = stack_argument(entry_esp, 2u);
    char format[CRT_FORMAT_LIMIT];
    char output[CRT_FORMAT_LIMIT];
    char string_argument[CRT_FORMAT_LIMIT];
    char second_string_argument[CRT_FORMAT_LIMIT];
    const char *directive;
    const char *second_directive = NULL;
    size_t length = 0u;
    RecompCrtFormatResult result;

    if (destination_address == 0u || format_address == 0u) {
        recomp_stop(2, "crt-format:null");
    }

    for (; length < sizeof format; ++length) {
        format[length] = (char)*recomp_memory_i8(
            format_address + (uint32_t)length);
        if (format[length] == '\0') {
            break;
        }
    }
    if (length == sizeof format) {
        recomp_stop(2, "crt-format:unterminated");
    }

    directive = strchr(format, '%');
    if (directive == NULL) {
        result = recomp_crt_format_literal(
            output, sizeof output, format, &length);
    } else if (directive[1] == 'd' &&
               strchr(directive + 2, '%') == NULL) {
        int32_t argument;

        if (argument_list_address == 0u) {
            recomp_stop(2, "crt-format:integer-args-null");
        }
        argument = (int32_t)*recomp_memory_u32(argument_list_address);
        result = recomp_crt_format_one_signed_decimal(
            output, sizeof output, format, argument, &length);
    } else if (directive[1] == 's') {
        uint32_t string_address;

        if (argument_list_address == 0u) {
            recomp_stop(2, "crt-format:string-args-null");
        }
        string_address = *recomp_memory_u32(argument_list_address);
        if (string_address == 0u) {
            recomp_stop(2, "crt-format:string-null");
        }
        if (!read_guest_string(
                string_address, string_argument, sizeof string_argument)) {
            recomp_stop(2, "crt-format:string-unterminated");
        }
        second_directive = strchr(directive + 2, '%');
        if (second_directive == NULL) {
            result = recomp_crt_format_one_string(
                output, sizeof output, format, string_argument, &length);
        } else if (second_directive[1] == 's' &&
                   strchr(second_directive + 2, '%') == NULL) {
            uint32_t second_string_address =
                *recomp_memory_u32(argument_list_address + 4u);

            if (second_string_address == 0u) {
                recomp_stop(2, "crt-format:second-string-null");
            }
            if (!read_guest_string(
                    second_string_address,
                    second_string_argument,
                    sizeof second_string_argument)) {
                recomp_stop(2, "crt-format:second-string-unterminated");
            }
            result = recomp_crt_format_two_strings(
                output,
                sizeof output,
                format,
                string_argument,
                second_string_argument,
                &length);
        } else if (second_directive[1] == 'd' &&
                   strchr(second_directive + 2, '%') == NULL) {
            int32_t signed_argument =
                (int32_t)*recomp_memory_u32(argument_list_address + 4u);

            result = recomp_crt_format_string_signed_decimal(
                output,
                sizeof output,
                format,
                string_argument,
                signed_argument,
                &length);
        } else {
            result = RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE;
        }
    } else {
        result = RECOMP_CRT_FORMAT_UNSUPPORTED_DIRECTIVE;
    }
    if (result != RECOMP_CRT_FORMAT_OK) {
        recomp_stop(2, "crt-format:model:%u", (unsigned)result);
    }

    for (size_t i = 0u; i <= length; ++i) {
        *recomp_memory_i8(destination_address + (uint32_t)i) =
            (int8_t)output[i];
    }
    if (directive == NULL && !reported_literal_path) {
        fprintf(stderr, "recomp crt: vsprintf literal-only length=%zu\n", length);
        reported_literal_path = 1;
    }
    if (directive != NULL && directive[1] == 'd' &&
        !reported_signed_decimal_path) {
        fprintf(
            stderr,
            "recomp crt: vsprintf signed-decimal length=%zu\n",
            length);
        reported_signed_decimal_path = 1;
    }
    if (directive != NULL && directive[1] == 's' &&
        second_directive == NULL &&
        !reported_string_path) {
        fprintf(stderr, "recomp crt: vsprintf one-string length=%zu\n", length);
        reported_string_path = 1;
    }
    if (second_directive != NULL && second_directive[1] == 's' &&
        !reported_two_string_path) {
        fprintf(stderr, "recomp crt: vsprintf two-string length=%zu\n", length);
        reported_two_string_path = 1;
    }
    if (second_directive != NULL && second_directive[1] == 'd' &&
        !reported_string_decimal_path) {
        fprintf(
            stderr,
            "recomp crt: vsprintf string-decimal length=%zu\n",
            length);
        reported_string_decimal_path = 1;
    }

    recomp_runtime.registers.eax = (uint32_t)length;
    recomp_runtime.registers.esp = entry_esp + 4u;
}

RecompFunction recomp_crt_format_lookup_manual(uint32_t guest_address)
{
    return guest_address == CRT_VSPRINTF_ADDRESS
        ? recomp_crt_vsprintf_adapter
        : NULL;
}
