#ifndef DOAXBV_RECOMP_D3D_FRAME_ADAPTER_H
#define DOAXBV_RECOMP_D3D_FRAME_ADAPTER_H

#include "d3d_presenter.h"
#include "runtime.h"

#include <stdint.h>

void recomp_d3d_frame_adapter_initialize(
    const RecompD3dPresenterConfig *config,
    uint32_t device_address);
void recomp_d3d_frame_adapter_reset(void);
void recomp_d3d_frame_adapter_reset_buffers(void);
void recomp_d3d_clear_adapter(void);
void recomp_d3d_swap_adapter(void);
RecompFunction recomp_d3d_frame_lookup_manual(uint32_t guest_address);

#endif
