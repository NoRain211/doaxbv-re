#include "kernel_abi.h"
#include "stop_report.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/* The game's own diagnostics. The native host stubbed DbgPrint out, so this
   is the first time DOAXBV can report what it thinks is going wrong. */

static void copy_guest_string(uint32_t guest_address, char *out, size_t size)
{
    size_t i = 0;

    if (guest_address == 0u) {
        snprintf(out, size, "(null)");
        return;
    }
    while (i + 1u < size) {
        char c = (char)*recomp_memory_i8(guest_address + (uint32_t)i);

        if (c == '\0') {
            break;
        }
        out[i++] = c;
    }
    out[i] = '\0';
}

/* Expands the subset of printf the XDK's DbgPrint callers actually use.
   Width and precision fields are skipped rather than honoured; the point is
   to make the message readable, not to reproduce formatting exactly. */
static void print_guest_format(uint32_t format_address, unsigned first_argument)
{
    char format[512];
    unsigned argument = first_argument;
    size_t i = 0;

    copy_guest_string(format_address, format, sizeof format);
    fputs("recomp game: ", stderr);
    while (format[i] != '\0') {
        if (format[i] != '%') {
            fputc(format[i++], stderr);
            continue;
        }

        ++i;
        if (format[i] == '%') {
            fputc('%', stderr);
            ++i;
            continue;
        }
        while (format[i] != '\0' &&
               (format[i] == '-' || format[i] == '+' || format[i] == ' ' ||
                format[i] == '#' || format[i] == '.' || format[i] == 'l' ||
                (format[i] >= '0' && format[i] <= '9'))) {
            ++i;
        }

        switch (format[i]) {
        case 's': {
            char text[512];

            copy_guest_string(kernel_arg(argument++), text, sizeof text);
            fputs(text, stderr);
            break;
        }
        case 'd':
        case 'i':
            fprintf(stderr, "%" PRId32, (int32_t)kernel_arg(argument++));
            break;
        case 'u':
            fprintf(stderr, "%" PRIu32, kernel_arg(argument++));
            break;
        case 'x':
            fprintf(stderr, "%" PRIx32, kernel_arg(argument++));
            break;
        case 'X':
            fprintf(stderr, "%" PRIX32, kernel_arg(argument++));
            break;
        case 'p':
            fprintf(stderr, "0x%08" PRIx32, kernel_arg(argument++));
            break;
        case 'c':
            fputc((int)(kernel_arg(argument++) & 0xffu), stderr);
            break;
        case '\0':
            continue;
        default:
            fputc('%', stderr);
            fputc(format[i], stderr);
            break;
        }
        ++i;
    }
    fputc('\n', stderr);
}

/* cdecl varargs: the caller cleans up, so only the return address is popped. */
static void bridge_dbg_print(void)
{
    print_guest_format(kernel_arg(1u), 2u);
    kernel_return_caller_cleanup(0u);
}

static void bridge_dbg_break_point(void)
{
    /* A debugger hook with no debugger attached. Report and keep running;
       the Xbox kernel does the same when nothing is listening. */
    fprintf(stderr, "recomp kernel: DbgBreakPoint ignored, no debugger\n");
    recomp_runtime.registers.esp += 4u;
}

static void bridge_ke_bug_check(void)
{
    uint32_t code = kernel_arg(1u);

    fprintf(
        stderr,
        "recomp kernel: KeBugCheck 0x%08" PRIx32 " - the game aborted\n",
        code);
    recomp_stop(2, "bugcheck:0x%08" PRIx32, code);
}

static void bridge_hal_return_to_firmware(void)
{
    uint32_t routine = kernel_arg(1u);

    /* The caller pushed a dummy return address of 0 and tail-jumped here, so
       the game does not expect to continue. On this XBE the only path that
       reaches it during bring-up is XapiBootToDash, the startup error and
       dashboard boundary, so this is a failure stop, not a clean halt. It
       exited 0 once and that made a failed run look like a success. */
    fprintf(
        stderr,
        "recomp kernel: HalReturnToFirmware routine=%" PRIu32
        " - guest returned to the firmware (dashboard/error boundary)\n",
        routine);
    recomp_stop(2, "firmware-return:%" PRIu32, routine);
}

RecompFunction recomp_kernel_debug(uint32_t ordinal)
{
    switch (ordinal) {
    case 5u: return bridge_dbg_break_point;
    case 8u: return bridge_dbg_print;
    case 49u: return bridge_hal_return_to_firmware;
    case 95u: return bridge_ke_bug_check;
    default: return NULL;
    }
}
