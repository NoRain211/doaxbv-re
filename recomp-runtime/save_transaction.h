#ifndef RECOMP_SAVE_TRANSACTION_H
#define RECOMP_SAVE_TRANSACTION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Recover before guest startup. Protects process interruption, not power loss.
   On Windows, hold exclusive ownership through save-undo-v1/lock until exit
   or reinitialization. The reserved lock file is empty and persists on disk.
   Callers must close UDATA handles before ending an operation or recovering. */
bool recomp_save_initialize(const char *disc_root);
bool recomp_save_begin(uint32_t owner);
/* False means an aborted operation or an error; never continue guest writes
   after an error. A nested failure also makes the outer operation abort. */
bool recomp_save_end(uint32_t owner, bool success);
void recomp_save_note_failure(uint32_t owner);
bool recomp_save_active(uint32_t owner);
bool recomp_save_pending(void);

#ifdef __cplusplus
}
#endif
#endif
