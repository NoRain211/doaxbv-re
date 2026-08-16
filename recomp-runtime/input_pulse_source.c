#include "input_pulse_source.h"

void recomp_input_pulse_source_init(
    RecompInputPulseSource *source,
    RecompInputSampleSource base,
    uint64_t pulse_poll)
{
    if (source == NULL) {
        return;
    }
    source->base = base;
    source->pulse_poll_count = 0u;
    source->sample_count = 0u;
    (void)recomp_input_pulse_source_add_poll(source, pulse_poll);
}

bool recomp_input_pulse_source_add_poll(
    RecompInputPulseSource *source,
    uint64_t pulse_poll)
{
    if (source == NULL || pulse_poll == 0u ||
        source->pulse_poll_count >= RECOMP_INPUT_PULSE_POLL_CAPACITY) {
        return false;
    }
    source->pulse_polls[source->pulse_poll_count] = pulse_poll;
    ++source->pulse_poll_count;
    return true;
}

bool recomp_input_pulse_source_sample(
    RecompInputPulseSource *source,
    RecompInputGamepad *gamepad)
{
    if (source == NULL || gamepad == NULL) {
        return false;
    }
    if (source->base == NULL) {
        *gamepad = (RecompInputGamepad){0};
    } else if (!source->base(gamepad)) {
        return false;
    }
    ++source->sample_count;
    for (unsigned i = 0u; i < source->pulse_poll_count; ++i) {
        if (source->sample_count == source->pulse_polls[i]) {
            gamepad->buttons |= RECOMP_INPUT_BUTTON_START;
            break;
        }
    }
    return true;
}
