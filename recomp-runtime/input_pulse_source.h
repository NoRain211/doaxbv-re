#ifndef DOAXBV_RECOMP_INPUT_PULSE_SOURCE_H
#define DOAXBV_RECOMP_INPUT_PULSE_SOURCE_H

#include "input_adapter.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    RECOMP_INPUT_BUTTON_START = 0x0010u,
    RECOMP_INPUT_PULSE_POLL_CAPACITY = 4u,
};

typedef struct RecompInputPulseSource {
    RecompInputSampleSource base;
    uint64_t pulse_polls[RECOMP_INPUT_PULSE_POLL_CAPACITY];
    unsigned pulse_poll_count;
    uint64_t sample_count;
} RecompInputPulseSource;

void recomp_input_pulse_source_init(
    RecompInputPulseSource *source,
    RecompInputSampleSource base,
    uint64_t pulse_poll);
/* The title spends its first press leaving the boot state, so the state that
   consumes START only sees an edge when a later poll is armed as well. */
bool recomp_input_pulse_source_add_poll(
    RecompInputPulseSource *source,
    uint64_t pulse_poll);
bool recomp_input_pulse_source_sample(
    RecompInputPulseSource *source,
    RecompInputGamepad *gamepad);

#ifdef __cplusplus
}
#endif

#endif
