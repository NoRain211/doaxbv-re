#include "program_manual.h"
#include "cri_service_adapter.h"
#include "crt_format_adapter.h"
#include "d3d_creation_adapter.h"
#ifdef RECOMP_D3D_FRAME_ENABLED
#include "d3d_frame_adapter.h"
#endif
#include "d3d_render_state_adapter.h"
#include "d3d_texture_adapter.h"
#include "d3d_tile_adapter.h"
#include "d3d_vertex_shader_adapter.h"
#include "dsound_service_adapter.h"
#include "fiber_adapter.h"
#include "input_adapter.h"
#include "stop_report.h"

#include <inttypes.h>
#include <stdio.h>

#ifdef RECOMP_FULL_PROGRAM
void sub_0006AFD0(void);
#endif
void sub_0018322D(void);

#ifdef RECOMP_FULL_PROGRAM
static void recomp_start_consumer_adapter(void)
{
    static int reported;
    uint32_t packet = *recomp_memory_u32(0x005f2e99u);
    uint32_t current = *recomp_memory_u32(0x005f3138u);
    uint32_t rising = *recomp_memory_u32(0x005f3140u);

    if (!reported && (rising & 0x10u) != 0u) {
        reported = 1;
        fprintf(
            stderr,
            "recomp input: START consumer packet=0x%08" PRIx32
            " current=0x%08" PRIx32 " rising=0x%08" PRIx32 "\n",
            packet,
            current,
            rising);
        recomp_stop_at_boundary("input-start-consumer:first");
    }
    sub_0006AFD0();
}
#endif

RecompFunction recomp_lookup_manual(uint32_t guest_address)
{
    RecompFunction function = recomp_cri_service_lookup_manual(guest_address);

    if (function == NULL) {
        function = recomp_d3d_lookup_manual(guest_address);
    }
    if (function == NULL) {
        function = recomp_d3d_render_state_lookup_manual(guest_address);
    }
    if (function == NULL) {
        function = recomp_d3d_texture_lookup_manual(guest_address);
    }
    if (function == NULL) {
        function = recomp_d3d_tile_lookup_manual(guest_address);
    }
    if (function == NULL) {
        function = recomp_d3d_vertex_shader_lookup_manual(guest_address);
    }
#ifdef RECOMP_D3D_FRAME_ENABLED
    if (function == NULL) {
        function = recomp_d3d_frame_lookup_manual(guest_address);
    }
#endif
    if (function == NULL) {
        function = recomp_dsound_service_lookup_manual(guest_address);
    }
    if (function == NULL) {
        function = recomp_input_lookup_manual(guest_address);
    }
    if (function == NULL) {
        function = recomp_crt_format_lookup_manual(guest_address);
    }
    if (function == NULL) {
        function = recomp_fiber_lookup_manual(guest_address);
    }
#ifdef RECOMP_FULL_PROGRAM
    if (function == NULL && guest_address == 0x0006afd0u) {
        function = recomp_start_consumer_adapter;
    }
#endif
    if (function == NULL && guest_address == 0x0018322du) {
        function = sub_0018322D;
    }
    return function;
}
