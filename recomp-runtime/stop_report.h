#ifndef DOAXBV_RECOMP_STOP_REPORT_H
#define DOAXBV_RECOMP_STOP_REPORT_H

#include <stdint.h>

/* Every terminal path reports here instead of calling exit() directly, so a
   run asserts where it stopped rather than a human reading the log and
   deciding. Without --expect-stop the historical exit codes are preserved.
   With it, the run exits 0 only when the stop matches, and 3 otherwise.

   Stop identifiers, matched by prefix so "import:" accepts any import stop:

     import:<Name>         unimplemented kernel import
     indirect:0x%08x       unresolved indirect call
     memory:0x%08x         undeclared guest memory access
     missing-body:0x%08x   referenced sub_* with no generated body
     breakpoint:<member>   generated __debugbreak
     bugcheck:0x%08x       guest called KeBugCheck
     firmware-return:<n>   guest handed control to the firmware
     thread-outside        thread termination outside a thread
     critical-section:...  contended or invalid critical section
     access-log-full       runtime access log exhausted
     completed             entry point returned normally
*/

#ifdef __cplusplus
extern "C" {
#endif

void recomp_stop_configure(const char *expected, const char *milestone_log);
void recomp_stop_configure_boundary(const char *stop_id);
void recomp_stop_at_boundary(const char *stop_id);

/* Records the stop, appends a milestone line when configured, then exits.
   fallback_code applies only when no expectation was set. Does not return. */
void recomp_stop(int fallback_code, const char *stop_id_format, ...);

/* Coarse forward-progress measure carried in the milestone line. */
void recomp_stop_note_kernel_call(void);

#ifdef __cplusplus
}
#endif

#endif
