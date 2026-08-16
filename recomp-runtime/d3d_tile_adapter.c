#include "d3d_tile_adapter.h"
#include "d3d_tile_model.h"
#include "stop_report.h"

#include <inttypes.h>
#include <stdio.h>

enum {
    D3D_DEVICE_SET_TILE_ADDRESS = 0x001e4930u,
    D3D_DEVICE_GLOBAL = 0x001f2978u,
};

static uint32_t stack_argument(uint32_t entry_esp, uint32_t index)
{
    return *recomp_memory_u32(entry_esp + 4u + index * 4u);
}

void recomp_d3d_set_tile_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t index = stack_argument(entry_esp, 0u);
    uint32_t source_address = stack_argument(entry_esp, 1u);
    uint32_t device_address = *recomp_memory_u32(D3D_DEVICE_GLOBAL);
    uint32_t destination_address;
    RecompD3dTile source = {0};
    RecompD3dTile destination;
    const RecompD3dTile *source_pointer = NULL;

    if (!recomp_d3d_tile_entry_address(
            device_address, index, &destination_address)) {
        fprintf(
            stderr,
            "recomp d3d: SetTile rejected index 0x%08" PRIx32
            " with device 0x%08" PRIx32 "\n",
            index,
            device_address);
        recomp_stop(2, "d3d-set-tile:0x%08" PRIx32, index);
    }

    if (source_address != 0u) {
        /* The source pointer arrives unchecked; words 1 and 5 are read at
           +4 and +20, so the whole tile must fit before anything is read. */
        if (!recomp_d3d_tile_range_fits(source_address)) {
            fprintf(
                stderr,
                "recomp d3d: SetTile rejected source 0x%08" PRIx32
                " with index 0x%08" PRIx32 "\n",
                source_address,
                index);
            recomp_stop(
                2, "d3d-set-tile-source:0x%08" PRIx32, source_address);
        }
        source.words[1] = *recomp_memory_u32(source_address + 4u);
        source_pointer = &source;
        if (source.words[1] != 0u) {
            for (uint32_t i = 0u; i < RECOMP_D3D_TILE_WORD_COUNT; ++i) {
                source.words[i] = *recomp_memory_u32(
                    source_address + i * 4u);
            }
        }
    }

    recomp_d3d_set_tile(&destination, source_pointer);
    for (uint32_t i = 0u; i < RECOMP_D3D_TILE_WORD_COUNT; ++i) {
        *recomp_memory_u32(destination_address + i * 4u) =
            destination.words[i];
    }
    recomp_runtime.registers.esp = entry_esp + 12u;
}

RecompFunction recomp_d3d_tile_lookup_manual(uint32_t guest_address)
{
    return guest_address == D3D_DEVICE_SET_TILE_ADDRESS
        ? recomp_d3d_set_tile_adapter
        : NULL;
}
