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
    RECOMP_INPUT_ANALOG_A = 0u,
    RECOMP_INPUT_ANALOG_PRESSED = 0xffu,
    /* 192, not 32: a scripted menu walk needs far more events than a fixed
       cadence. At 32 the runner rejects the whole command line, so a schedule
       that outlives the run window cannot be expressed at all. The arrays are
       members of a single static RecompInputPulseSource in runner.cpp, so the
       cost is a few KB of BSS. */
    RECOMP_INPUT_A_PULSE_POLL_CAPACITY = 192u,
    RECOMP_INPUT_BUTTONS_PULSE_CAPACITY = 192u,
};

/* Guest ordering, from xinput_xbox.h:46 -- "A, B, X, Y, Black, White, LTrig,
   RTrig". Not the ordering in the keyboard comments in input_host_win32.c,
   which are a permuted host binding that reads 0=X, 1=Y, 2=A, 3=B and also
   swaps Black and White. RECOMP_INPUT_ANALOG_A = 0 provably delivers A, which
   settles it. */
enum {
    RECOMP_INPUT_ANALOG_B = 1u,
    RECOMP_INPUT_ANALOG_X = 2u,
    RECOMP_INPUT_ANALOG_Y = 3u,
    RECOMP_INPUT_ANALOG_BLACK = 4u,
    RECOMP_INPUT_ANALOG_WHITE = 5u,
    RECOMP_INPUT_ANALOG_LTRIG = 6u,
    RECOMP_INPUT_ANALOG_RTRIG = 7u,
    RECOMP_INPUT_ANALOG_PULSE_CAPACITY = 192u,
};

typedef struct RecompInputPulseSource {
    RecompInputSampleSource base;
    uint64_t pulse_polls[RECOMP_INPUT_PULSE_POLL_CAPACITY];
    unsigned pulse_poll_count;
    uint64_t a_polls[RECOMP_INPUT_A_PULSE_POLL_CAPACITY];
    unsigned a_poll_count;
    uint64_t buttons_polls[RECOMP_INPUT_BUTTONS_PULSE_CAPACITY];
    uint16_t buttons_masks[RECOMP_INPUT_BUTTONS_PULSE_CAPACITY];
    unsigned buttons_poll_count;
    uint64_t analog_polls[RECOMP_INPUT_ANALOG_PULSE_CAPACITY];
    uint8_t analog_indices[RECOMP_INPUT_ANALOG_PULSE_CAPACITY];
    uint8_t analog_values[RECOMP_INPUT_ANALOG_PULSE_CAPACITY];
    unsigned analog_poll_count;
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
/* The A button is analog index 0, not a digital bit: the title reads it
   through the analog byte array and treats anything >= 0x20 as pressed. */
bool recomp_input_pulse_source_add_a_poll(
    RecompInputPulseSource *source,
    uint64_t pulse_poll);
/* Asserts an arbitrary digital button mask so dialogs and back/cancel
   paths can be driven without hand-wiring each button's analog index. */
bool recomp_input_pulse_source_add_buttons_poll(
    RecompInputPulseSource *source,
    uint64_t pulse_poll,
    uint16_t mask);
/* Presses one analog button by guest index. The A pulse above is the index-0
   special case that predates this; both write the same array. */
bool recomp_input_pulse_source_add_analog_poll(
    RecompInputPulseSource *source,
    uint64_t pulse_poll,
    uint8_t index,
    uint8_t value);
bool recomp_input_pulse_source_sample(
    RecompInputPulseSource *source,
    RecompInputGamepad *gamepad);

#ifdef __cplusplus
}
#endif

#endif
