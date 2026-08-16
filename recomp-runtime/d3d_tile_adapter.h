#ifndef DOAXBV_RECOMP_D3D_TILE_ADAPTER_H
#define DOAXBV_RECOMP_D3D_TILE_ADAPTER_H

#include "runtime.h"

void recomp_d3d_set_tile_adapter(void);
RecompFunction recomp_d3d_tile_lookup_manual(uint32_t guest_address);

#endif
