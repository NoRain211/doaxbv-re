/* Audit regression: incomplete remote input must not complete a 65-row seal.
   Build/run commands and the affected pin are in rollback-engine-audit.md. */
#include "recomp_net/rollback.h"

#include <stdio.h>

static int unused_step(void *ctx, uint32_t tick)
{
    (void)ctx;
    (void)tick;
    return 0;
}

static uint32_t unused_digest(void *ctx, uint32_t tick, uint32_t partition)
{
    (void)ctx;
    (void)tick;
    (void)partition;
    return 0;
}

static uint8_t local_input(void *ctx, int32_t slot, uint32_t tick,
                           RNetRbFrame *out)
{
    (void)ctx;
    *out = (RNetRbFrame){0};
    if (slot != 0)
        return 0;
    out->tick = tick;
    out->buttons = 0xffffu;
    out->is_valid = 1;
    return 1;
}

int main(void)
{
    RNetRbConfig config = {0};
    RNetRollbackVTable host = {0};
    RNetRbCorrection correction = {0};
    RNetRbSession *session;
    int complete;
    int last_authoritative;

    config.slot_count = 2;
    host.save_state = unused_step;
    host.load_state = unused_step;
    host.advance_sim = unused_step;
    host.state_digest = unused_digest;
    host.get_input_row = local_input;
    session = rnet_rb_create(&config, &host);
    if (session == NULL)
        return 2;
    correction.epoch_id = 1;
    correction.load_tick = correction.mismatch_tick = 100;
    correction.target_tick = 164;
    correction.slot = 1;
    correction.initiator = 1;
    rnet_rb_begin_episode(session, &correction);
    rnet_rb_seal_inputs(session, 100, 164, 1);
    if (!rnet_rb_inputs_sealed(session) &&
        rnet_rb_get_seal_span(session) == 0 &&
        rnet_rb_get_target_tick(session) == 164) {
        puts("PASS: oversized seal rejected without shortening target");
        rnet_rb_destroy(session);
        return 0;
    }
    if (rnet_rb_get_seal_span(session) <= 64) {
        fputs("FAIL: oversized seal must not silently shorten its target\n", stderr);
        rnet_rb_destroy(session);
        return 1;
    }
    for (uint32_t offset = 0; offset < 64; ++offset) {
        RNetRbFrame row = {0};
        row.tick = 100 + offset;
        row.buttons = 0xffffu;
        row.is_valid = 1;
        if (!rnet_rb_apply_peer_seal_rows(session, 1, 100, 164, 1,
                                          offset, &row, 1)) {
            rnet_rb_destroy(session);
            return 2;
        }
    }
    complete = rnet_rb_all_peer_seal_rows_complete(session);
    last_authoritative = rnet_rb_seat_row_authoritative(session, 1, 164);
    printf("seal_span=%u complete=%d last_authoritative=%d\n",
           rnet_rb_get_seal_span(session), complete, last_authoritative);
    rnet_rb_destroy(session);
    if (complete || last_authoritative) {
        fputs("FAIL: missing last row must keep the seal incomplete\n", stderr);
        return 1;
    }
    return 0;
}
