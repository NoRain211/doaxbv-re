#ifndef DOAXBV_RECOMP_D3D_RENDER_STATE_ADAPTER_H
#define DOAXBV_RECOMP_D3D_RENDER_STATE_ADAPTER_H

#include "d3d_render_state_model.h"
#include "runtime.h"

void recomp_d3d_set_normalize_normals_adapter(void);
void recomp_d3d_set_texture_factor_adapter(void);
void recomp_d3d_set_cull_mode_adapter(void);
void recomp_d3d_set_multisample_antialias_adapter(void);
void recomp_d3d_set_stencil_enable_adapter(void);
void recomp_d3d_set_z_enable_adapter(void);
void recomp_d3d_set_fill_mode_adapter(void);
void recomp_d3d_set_edge_antialias_adapter(void);
void recomp_d3d_set_simple_render_state_adapter(void);
RecompFunction recomp_d3d_render_state_lookup_manual(uint32_t guest_address);

/* Test seam: reset and inspect the file-static model. */
void recomp_d3d_render_state_adapter_reset(void);
const RecompD3dRenderStateModel *recomp_d3d_render_state_adapter_model(void);

/* Reports the render states the guest actually set, so a consumer wiring
   state to the host can see which ones this title uses rather than guess. */
void recomp_d3d_render_state_adapter_report(void);

#endif
