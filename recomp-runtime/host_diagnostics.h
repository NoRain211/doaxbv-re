#ifndef DOAXBV_RECOMP_HOST_DIAGNOSTICS_H
#define DOAXBV_RECOMP_HOST_DIAGNOSTICS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Windows unhandled exceptions emit host-crash events in Release and Debug.
   Debug additionally logs observed exceptions (terminal=unknown) and continuing
   CRT runtime checks (terminal=false); neither alone establishes a crash. */
void recomp_install_host_diagnostics(void);

#ifdef __cplusplus
}
#endif

#endif
