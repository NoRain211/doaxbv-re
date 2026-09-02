#ifndef DOAXBV_RECOMP_D3D_TEXTURE_ADAPTER_H
#define DOAXBV_RECOMP_D3D_TEXTURE_ADAPTER_H

#include "d3d_texture_model.h"
#include "runtime.h"

RecompFunction recomp_d3d_texture_lookup_manual(uint32_t guest_address);

void recomp_d3d_texture_adapter_reset(void);
const RecompD3dTextureModel *recomp_d3d_texture_adapter_model(void);
const RecompD3dTextureDesc *recomp_d3d_texture_adapter_stage(uint32_t stage);
void recomp_d3d_texture_adapter_report(void);

#endif
