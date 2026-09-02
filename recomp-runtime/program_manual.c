#include "program_manual.h"
#include "cri_service_adapter.h"
#include "crt_format_adapter.h"
#include "d3d_creation_adapter.h"
#ifdef RECOMP_D3D_FRAME_ENABLED
#include "d3d_draw_adapter.h"
#endif
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
#include <stdlib.h>

#ifdef RECOMP_FULL_PROGRAM
void sub_0006AFD0(void);
#endif
void sub_0018322D(void);
#ifdef RECOMP_FULL_PROGRAM
void sub_0011F250(void);
#endif

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

#ifdef RECOMP_FULL_PROGRAM
/* Round 30. sub_0011F250 is the per-view frame entry: it takes a view index
   and uses it to select both the camera block (0x009EEE70 + index * 0x1B0)
   and the per-view texture tables (0xB2D680 + index * 0xA00). Round 21
   showed two smooth camera streams alternating between frames and asked what
   supplies that index; rounds 22-26 then sampled globals believed to feed it
   and found all of them constant.

   The argument itself was never read. At entry it is the dword above the
   return slot, so observe it here and chain to the generated body, the same
   shape as recomp_start_consumer_adapter. The chain call is unconditional,
   so behaviour is identical whether or not the probe is enabled. */
static void recomp_view_entry_adapter(void)
{
    static const char *view_arg_trace;
    static bool view_arg_trace_read;
    static uint32_t view_arg_lines;

    if (!view_arg_trace_read) {
        view_arg_trace_read = true;
        view_arg_trace = getenv("RECOMP_D3D_VIEWARG");
    }
    if (view_arg_trace != NULL && view_arg_lines < 240u) {
        const uint32_t *argument =
            recomp_memory_u32(recomp_runtime.registers.esp + 4u);

        if (argument != NULL) {
            ++view_arg_lines;
            fprintf(
                stderr,
                "recomp d3d viewarg: swap=%" PRIu32 " index=%" PRIu32
                "\n",
                recomp_d3d_frame_adapter_swap_counter(),
                *argument);
        }
    }
    sub_0011F250();
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
    if (function == NULL) {
        function = recomp_d3d_draw_lookup_manual(guest_address);
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
    if (function == NULL && guest_address == 0x0011f250u) {
        function = recomp_view_entry_adapter;
    }
#endif
    if (function == NULL && guest_address == 0x0018322du) {
        function = sub_0018322D;
    }
    return function;
}
