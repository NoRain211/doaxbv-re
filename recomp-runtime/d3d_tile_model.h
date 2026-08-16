#ifndef DOAXBV_RECOMP_D3D_TILE_MODEL_H
#define DOAXBV_RECOMP_D3D_TILE_MODEL_H

#include <stdbool.h>
#include <stdint.h>

enum {
    RECOMP_D3D_TILE_WORD_COUNT = 6u,
    RECOMP_D3D_TILE_COUNT = 8u,
    RECOMP_D3D_TILE_ARRAY_OFFSET = 0x00002260u,
    RECOMP_D3D_TILE_ENTRY_SIZE = 0x00000018u,
    /* Bytes one tile occupies, and bytes from the device base through the
       last byte of the last tile. Both bound guest-supplied arithmetic. */
    RECOMP_D3D_TILE_ENTRY_SPAN = RECOMP_D3D_TILE_WORD_COUNT * 4u,
    RECOMP_D3D_TILE_ARRAY_EXTENT =
        RECOMP_D3D_TILE_ARRAY_OFFSET +
        (RECOMP_D3D_TILE_COUNT - 1u) * RECOMP_D3D_TILE_ENTRY_SIZE +
        RECOMP_D3D_TILE_ENTRY_SPAN,
};

typedef struct RecompD3dTile {
    uint32_t words[RECOMP_D3D_TILE_WORD_COUNT];
} RecompD3dTile;

void recomp_d3d_set_tile(
    RecompD3dTile *destination,
    const RecompD3dTile *source);

/* Resolves the guest address of one tile entry. Returns false, leaving
   *address untouched, when the device global is null, the guest-supplied
   index is outside the eight-entry array, or the device base sits so high
   that the whole array would run past the end of the 32-bit guest address
   space. Both inputs arrive unchecked from the guest; the address is only
   computed once each term is bounded, so the sum cannot wrap. */
bool recomp_d3d_tile_entry_address(
    uint32_t device_address,
    uint32_t index,
    uint32_t *address);

/* Whether the six-DWORD tile at this guest address ends before the 32-bit
   address space does. The caller supplies an unchecked guest pointer, so
   this is asked before any word of the tile is read. */
bool recomp_d3d_tile_range_fits(uint32_t address);

#endif
