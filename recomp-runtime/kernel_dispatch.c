#include "kernel_abi.h"
#include "runtime.h"
#include "stop_report.h"

/* Encoded kernel thunks read as 0x8000xxxx, where xxxx is the ordinal.
   Each subsystem owns its own file; add a chain entry when you add one. */
RecompFunction recomp_lookup_kernel(uint32_t guest_address)
{
    uint32_t ordinal;
    RecompFunction function;

    if ((guest_address & 0xffff0000u) != 0x80000000u) {
        return NULL;
    }
    ordinal = guest_address & 0xffffu;

    function = recomp_kernel_startup(ordinal);
    if (function == NULL) {
        function = recomp_kernel_config(ordinal);
    }
    if (function == NULL) {
        function = recomp_kernel_debug(ordinal);
    }
    if (function == NULL) {
        function = recomp_kernel_device(ordinal);
    }
    if (function == NULL) {
        function = recomp_kernel_interrupt(ordinal);
    }
    if (function == NULL) {
        function = recomp_kernel_memory(ordinal);
    }
    if (function == NULL) {
        function = recomp_kernel_rtl(ordinal);
    }
    if (function == NULL) {
        function = recomp_kernel_thread(ordinal);
    }
    if (function == NULL) {
        function = recomp_kernel_video(ordinal);
    }
    if (function == NULL) {
        function = recomp_kernel_file(ordinal);
    }
    if (function != NULL) {
        recomp_stop_note_kernel_call();
    }
    return function;
}
