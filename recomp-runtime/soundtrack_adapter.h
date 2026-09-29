#ifndef DOAXBV_RECOMP_SOUNDTRACK_ADAPTER_H
#define DOAXBV_RECOMP_SOUNDTRACK_ADAPTER_H

#include "runtime.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Replaces the XAPI soundtrack catalog with the UserMusic library and the WMA
   XMO decoder with host PCM decoding. Every WMA XMO is created here; creation
   fails for a context with no bound song. Enumerator, context, and stop calls
   for handles or contexts this adapter does not own fall through to the
   generated originals. */
RecompFunction recomp_soundtrack_lookup_manual(uint32_t address);
/* Closes every host decoder; call before recomp_music_shutdown. Guest pool
   memory is left to the guest heap. */
void recomp_soundtrack_shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
