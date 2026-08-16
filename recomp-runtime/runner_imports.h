#ifndef DOAXBV_RECOMP_RUNNER_IMPORTS_H
#define DOAXBV_RECOMP_RUNNER_IMPORTS_H

#include <stddef.h>
#include <stdint.h>

typedef enum RecompImportKind {
    RECOMP_IMPORT_NONE,
    RECOMP_IMPORT_CREATE_THREAD,
    RECOMP_IMPORT_BOOT_TO_DASH,
    RECOMP_IMPORT_EXIT_THREAD,
} RecompImportKind;

typedef struct RecompImportState {
    RecompImportKind first_import;
    size_t create_thread_calls;
    size_t boot_to_dash_calls;
    size_t exit_thread_calls;
    uint32_t thread_start;
} RecompImportState;

#ifdef __cplusplus
extern "C" {
#endif

void recomp_imports_reset(void);
const RecompImportState *recomp_imports_state(void);

void sub_00182DC7(void);
void sub_00186396(void);
void sub_0018210D(void);

#ifdef __cplusplus
}
#endif

#endif
