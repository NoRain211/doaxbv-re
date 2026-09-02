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
    source->a_poll_count = 0u;
    source->buttons_poll_count = 0u;
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

bool recomp_input_pulse_source_add_a_poll(
    RecompInputPulseSource *source,
    uint64_t pulse_poll)
{
    if (source == NULL || pulse_poll == 0u ||
        source->a_poll_count >= RECOMP_INPUT_A_PULSE_POLL_CAPACITY) {
        return false;
    }
    source->a_polls[source->a_poll_count] = pulse_poll;
    ++source->a_poll_count;
    return true;
}

bool recomp_input_pulse_source_add_buttons_poll(
    RecompInputPulseSource *source,
    uint64_t pulse_poll,
    uint16_t mask)
{
    if (source == NULL || pulse_poll == 0u ||
        source->buttons_poll_count >= RECOMP_INPUT_BUTTONS_PULSE_CAPACITY) {
        return false;
    }
    source->buttons_polls[source->buttons_poll_count] = pulse_poll;
    source->buttons_masks[source->buttons_poll_count] = mask;
    ++source->buttons_poll_count;
    return true;
}

bool recomp_input_pulse_source_add_analog_poll(
    RecompInputPulseSource *source,
    uint64_t pulse_poll,
    uint8_t index,
    uint8_t value)
{
    if (source == NULL || pulse_poll == 0u ||
        index >= RECOMP_INPUT_ANALOG_BUTTON_COUNT ||
        source->analog_poll_count >= RECOMP_INPUT_ANALOG_PULSE_CAPACITY) {
        return false;
    }
    source->analog_polls[source->analog_poll_count] = pulse_poll;
    source->analog_indices[source->analog_poll_count] = index;
    source->analog_values[source->analog_poll_count] = value;
    ++source->analog_poll_count;
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
    for (unsigned i = 0u; i < source->a_poll_count; ++i) {
        if (source->sample_count == source->a_polls[i]) {
            gamepad->analog_buttons[RECOMP_INPUT_ANALOG_A] =
                RECOMP_INPUT_ANALOG_PRESSED;
            break;
        }
    }
    for (unsigned i = 0u; i < source->buttons_poll_count; ++i) {
        if (source->sample_count == source->buttons_polls[i]) {
            gamepad->buttons |= source->buttons_masks[i];
            break;
        }
    }
    for (unsigned i = 0u; i < source->analog_poll_count; ++i) {
        if (source->sample_count == source->analog_polls[i]) {
            gamepad->analog_buttons[source->analog_indices[i]] =
                source->analog_values[i];
            break;
        }
    }
    return true;
}
