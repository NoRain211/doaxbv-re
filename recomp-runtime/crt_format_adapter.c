#include "crt_format_adapter.h"

#include "crt_format_model.h"
#include "stop_report.h"

#include <stdio.h>

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

/* Non-reentrant by design: vsprintf never yields or re-enters. */
static char format_buffer[CRT_FORMAT_LIMIT];
static char output_buffer[CRT_FORMAT_LIMIT];
static char string_buffer[CRT_FORMAT_LIMIT];

/* Reads va_list slot index; the model consumes a %s copy before the next fetch. */
static int fetch_argument(
    void *context,
    size_t index,
    char conversion,
    RecompCrtFormatArgument *argument)
{
    uint32_t argument_list_address = *(const uint32_t *)context;
    static unsigned char reported[128];

    if (!reported[(unsigned char)conversion & 127u]) {
        reported[(unsigned char)conversion & 127u] = 1u;
        fprintf(stderr, "recomp crt: vsprintf first %%%c\n", conversion);
    }
    if (argument_list_address == 0u) {
        recomp_stop(2, "crt-format:args-null");
    }
    argument->value =
        *recomp_memory_u32(argument_list_address + (uint32_t)index * 4u);
    if (conversion == 's') {
        if (argument->value == 0u) {
            recomp_stop(2, "crt-format:string-null");
        }
        if (!read_guest_string(
                argument->value, string_buffer, sizeof string_buffer)) {
            recomp_stop(2, "crt-format:string-unterminated");
        }
        argument->string = string_buffer;
    }
    return 1;
}

static void recomp_crt_vsprintf_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t destination_address = stack_argument(entry_esp, 0u);
    uint32_t format_address = stack_argument(entry_esp, 1u);
    uint32_t argument_list_address = stack_argument(entry_esp, 2u);
    size_t length = 0u;
    RecompCrtFormatResult result;

    if (destination_address == 0u || format_address == 0u) {
        recomp_stop(2, "crt-format:null");
    }
    if (!read_guest_string(format_address, format_buffer, sizeof format_buffer)) {
        recomp_stop(2, "crt-format:unterminated");
    }

    result = recomp_crt_format(output_buffer, sizeof output_buffer,
        format_buffer, fetch_argument, &argument_list_address, &length);
    if (result != RECOMP_CRT_FORMAT_OK) {
        recomp_stop(2, "crt-format:model:%u", (unsigned)result);
    }
    for (size_t i = 0u; i <= length; ++i) {
        *recomp_memory_i8(destination_address + (uint32_t)i) =
            (int8_t)output_buffer[i];
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
