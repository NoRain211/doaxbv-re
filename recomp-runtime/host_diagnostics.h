#ifndef DOAXBV_RECOMP_HOST_DIAGNOSTICS_H
#define DOAXBV_RECOMP_HOST_DIAGNOSTICS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Routes host CRT runtime-check failures to stderr and names the generated
   function that raised them. Runtime checks stay enabled. */
void recomp_install_host_diagnostics(void);

#ifdef __cplusplus
}
#endif

#endif
