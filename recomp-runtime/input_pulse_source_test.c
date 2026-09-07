#include "input_pulse_source.h"

#include <stdio.h>
#include <string.h>

static RecompInputGamepad base_gamepad;
static int base_fail_count;

static bool sample_base(RecompInputGamepad *gamepad)
{
    if (base_fail_count > 0) {
        --base_fail_count;
        return false;
    }
    *gamepad = base_gamepad;
    return true;
}

static void reset_base(void)
{
    memset(&base_gamepad, 0, sizeof base_gamepad);
    base_fail_count = 0;
}

static int expect_u64(const char *field, uint64_t actual, uint64_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "input pulse source: %s was 0x%llx, expected 0x%llx\n",
        field,
        (unsigned long long)actual,
        (unsigned long long)expected);
    return 0;
}

static int expect_buttons(const char *field, uint16_t actual, uint16_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "input pulse source: %s was 0x%04x, expected 0x%04x\n",
        field,
        actual,
        expected);
    return 0;
}

static int expect_pad_zero(const char *field, const RecompInputGamepad *pad)
{
    static const RecompInputGamepad zero_pad;
    if (memcmp(pad, &zero_pad, sizeof zero_pad) == 0) {
        return 1;
    }
    fprintf(stderr, "input pulse source: %s was not all zero\n", field);
    return 0;
}

int recomp_input_pulse_source_test(void)
{
    RecompInputPulseSource source = {0};
    RecompInputGamepad pad;
    int passed = 1;

    /* neutral -> START on the pulse poll -> neutral again */
    reset_base();
    recomp_input_pulse_source_init(&source, sample_base, 2u);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons("poll 1 buttons", pad.buttons, 0u);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons(
        "poll 2 buttons", pad.buttons, RECOMP_INPUT_BUTTON_START);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons("poll 3 buttons", pad.buttons, 0u);
    passed &= expect_u64("poll 3 count", source.sample_count, 3u);

    /* pulse ORs into the composed base state without dropping it */
    reset_base();
    base_gamepad.buttons = 0x0001u;
    base_gamepad.analog_buttons[0] = 0x7fu;
    base_gamepad.thumb_lx = -100;
    base_gamepad.thumb_ry = 200;
    recomp_input_pulse_source_init(&source, sample_base, 1u);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons(
        "preserved buttons", pad.buttons, 0x0001u | RECOMP_INPUT_BUTTON_START);
    passed &= expect_u64("preserved analog", pad.analog_buttons[0], 0x7fu);
    passed &= expect_u64(
        "preserved lx", (uint64_t)pad.thumb_lx, (uint64_t)-100);
    passed &= expect_u64("preserved ry", (uint64_t)pad.thumb_ry, 200u);

    /* a failed base sample does not consume a poll */
    reset_base();
    base_fail_count = 1;
    recomp_input_pulse_source_init(&source, sample_base, 2u);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 0 : 1;
    passed &= expect_u64("failed count", source.sample_count, 0u);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons("retry buttons", pad.buttons, 0u);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons(
        "shifted pulse buttons", pad.buttons, RECOMP_INPUT_BUTTON_START);
    passed &= expect_u64("shifted count", source.sample_count, 2u);

    /* NULL base produces an all-zero pad and still counts samples */
    recomp_input_pulse_source_init(&source, NULL, 2u);
    memset(&pad, 0xa5, sizeof pad);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_pad_zero("null base poll 1", &pad);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons(
        "null base pulse", pad.buttons, RECOMP_INPUT_BUTTON_START);
    passed &= expect_u64("null base count", source.sample_count, 2u);

    /* Handover preserves the poll count and does not replay scripted input. */
    reset_base();
    base_gamepad.buttons = 0x0004u;
    base_gamepad.analog_buttons[RECOMP_INPUT_ANALOG_A] =
        RECOMP_INPUT_ANALOG_PRESSED;
    source.base = sample_base;
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_u64("handover count", source.sample_count, 3u);
    passed &= expect_buttons("handover host buttons", pad.buttons, 0x0004u);
    passed &= expect_u64(
        "handover host A", pad.analog_buttons[RECOMP_INPUT_ANALOG_A],
        RECOMP_INPUT_ANALOG_PRESSED);
    reset_base();
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_u64("handover release count", source.sample_count, 4u);
    passed &= expect_pad_zero("handover host release", &pad);

    /* Pause releases held input without consuming the next scripted poll. */
    recomp_input_pulse_source_init(&source, sample_base, 1u);
    passed &= recomp_input_pulse_source_add_a_poll(&source, 2u) ? 1 : 0;
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons(
        "pause boundary START", pad.buttons, RECOMP_INPUT_BUTTON_START);
    source.paused = true;
    base_fail_count = 1;
    for (unsigned i = 0u; i < 2u; ++i) {
        passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
        passed &= expect_pad_zero("paused release", &pad);
        passed &= expect_u64("paused count", source.sample_count, 1u);
    }
    passed &= expect_u64("paused base not called", base_fail_count, 1u);
    passed &= recomp_input_pulse_source_press_analog(
        NULL, RECOMP_INPUT_ANALOG_A) ? 0 : 1;
    passed &= recomp_input_pulse_source_press_analog(
        &source, RECOMP_INPUT_ANALOG_BUTTON_COUNT) ? 0 : 1;
    passed &= recomp_input_pulse_source_press_analog(
        &source, RECOMP_INPUT_ANALOG_A) ? 1 : 0;
    passed &= recomp_input_pulse_source_press_analog(
        &source, RECOMP_INPUT_ANALOG_B) ? 0 : 1;
    passed &= recomp_input_pulse_source_sample(&source, NULL) ? 0 : 1;
    for (unsigned i = 0u; i < 6u; ++i) {
        passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
        passed &= expect_u64(
            "paused live A", pad.analog_buttons[RECOMP_INPUT_ANALOG_A],
            RECOMP_INPUT_ANALOG_PRESSED);
        pad.analog_buttons[RECOMP_INPUT_ANALOG_A] = 0u;
        passed &= expect_pad_zero("paused live other input", &pad);
        passed &= expect_u64("paused live count", source.sample_count, 1u);
    }
    passed &= recomp_input_pulse_source_press_analog(
        &source, RECOMP_INPUT_ANALOG_A) ? 0 : 1;
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_pad_zero("paused live release", &pad);
    passed &= expect_u64("paused release count", source.sample_count, 1u);
    passed &= expect_u64("live paused base not called", base_fail_count, 1u);
    reset_base();
    source.paused = false;
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_u64("resumed count", source.sample_count, 2u);
    passed &= expect_buttons("resumed no START replay", pad.buttons, 0u);
    passed &= expect_u64(
        "resumed A", pad.analog_buttons[RECOMP_INPUT_ANALOG_A],
        RECOMP_INPUT_ANALOG_PRESSED);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_u64("resumed release count", source.sample_count, 3u);
    passed &= expect_pad_zero("resumed A release", &pad);

    /* A live pulse composes with normal input; failed samples do not spend it. */
    reset_base();
    base_gamepad.buttons = 0x0001u;
    base_gamepad.analog_buttons[RECOMP_INPUT_ANALOG_A] = 0x7fu;
    base_gamepad.analog_buttons[RECOMP_INPUT_ANALOG_B] = 0x40u;
    recomp_input_pulse_source_init(&source, sample_base, 2u);
    passed &= recomp_input_pulse_source_press_analog(
        &source, RECOMP_INPUT_ANALOG_B) ? 1 : 0;
    base_fail_count = 1;
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 0 : 1;
    passed &= expect_u64("live failed sample count", source.sample_count, 0u);
    for (unsigned i = 0u; i < 7u; ++i) {
        passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
        passed &= expect_u64(
            "normal live B", pad.analog_buttons[RECOMP_INPUT_ANALOG_B],
            i < 6u ? RECOMP_INPUT_ANALOG_PRESSED : 0u);
        passed &= expect_u64(
            "normal live base A", pad.analog_buttons[RECOMP_INPUT_ANALOG_A],
            0x7fu);
        passed &= expect_buttons("normal live buttons", pad.buttons,
            0x0001u | (i == 1u ? RECOMP_INPUT_BUTTON_START : 0u));
        passed &= expect_u64("normal live count", source.sample_count, i + 1u);
    }
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_u64("normal live base restored",
        pad.analog_buttons[RECOMP_INPUT_ANALOG_B], 0x40u);

    /* Reinitialization clears paused state and previously armed analog input. */
    passed &= recomp_input_pulse_source_add_analog_poll(
        &source, 1u, RECOMP_INPUT_ANALOG_B,
        RECOMP_INPUT_ANALOG_PRESSED) ? 1 : 0;
    passed &= recomp_input_pulse_source_press_analog(
        &source, RECOMP_INPUT_ANALOG_B) ? 1 : 0;
    source.paused = true;
    recomp_input_pulse_source_init(&source, NULL, 0u);
    passed &= expect_u64("reset paused", source.paused, 0u);
    passed &= expect_u64("reset analog count", source.analog_poll_count, 0u);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_u64("reset sample count", source.sample_count, 1u);
    passed &= expect_pad_zero("reset analog release", &pad);

    /* Digital commands share the live pulse slot and release only their bits. */
    recomp_input_pulse_source_init(&source, sample_base, 0u);
    source.paused = true;
    base_fail_count = 1;
    passed &= recomp_input_pulse_source_press_buttons(NULL, 8u) ? 0 : 1;
    passed &= recomp_input_pulse_source_press_buttons(&source, 0u) ? 0 : 1;
    passed &= recomp_input_pulse_source_press_buttons(&source, 0x100u) ? 0 : 1;
    passed &= recomp_input_pulse_source_press_analog(&source, 0u) ? 1 : 0;
    passed &= recomp_input_pulse_source_press_buttons(&source, 8u) ? 0 : 1;
    for (unsigned i = 0u; i < 7u; ++i) {
        passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    }
    passed &= recomp_input_pulse_source_press_buttons(&source, 8u) ? 1 : 0;
    passed &= recomp_input_pulse_source_sample(&source, NULL) ? 0 : 1;
    for (unsigned i = 0u; i < 7u; ++i) {
        passed &= recomp_input_pulse_source_press_analog(&source, 0u) ? 0 : 1;
        passed &= recomp_input_pulse_source_press_buttons(&source, 4u) ? 0 : 1;
        passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
        passed &= expect_buttons("paused digital pulse", pad.buttons, i < 6u ? 8u : 0u);
        pad.buttons = 0u;
        passed &= expect_pad_zero("paused digital other input", &pad);
        passed &= expect_u64("paused digital count", source.sample_count, 0u);
    }
    passed &= expect_u64("paused digital base bypass", base_fail_count, 1u);
    passed &= recomp_input_pulse_source_press_analog(&source, 0u) ? 1 : 0;
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons("analog after digital", pad.buttons, 0u);
    passed &= expect_u64("analog after digital A", pad.analog_buttons[0], 0xffu);

    reset_base();
    base_gamepad.buttons = 9u;
    base_gamepad.analog_buttons[0] = 0x7fu;
    recomp_input_pulse_source_init(&source, sample_base, 2u);
    passed &= recomp_input_pulse_source_press_buttons(&source, 8u) ? 1 : 0;
    base_fail_count = 1;
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 0 : 1;
    passed &= expect_u64("digital failed sample retained", source.pending_samples, 7u);
    for (unsigned i = 0u; i < 7u; ++i) {
        passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
        passed &= expect_buttons("digital preserves base and script", pad.buttons,
            (i < 6u ? 9u : 1u) | (i == 1u ? RECOMP_INPUT_BUTTON_START : 0u));
        passed &= expect_u64("digital preserves analog", pad.analog_buttons[0], 0x7fu);
    }
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons("digital base restored", pad.buttons, 9u);
    passed &= recomp_input_pulse_source_press_buttons(&source, 8u) ? 1 : 0;
    recomp_input_pulse_source_init(&source, NULL, 0u);
    passed &= expect_u64("reset digital countdown", source.pending_samples, 0u);
    passed &= expect_u64("reset digital mask", source.pending_buttons_mask, 0u);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_pad_zero("reset digital release", &pad);

    /* NULL source and gamepad pointers fail safely */
    recomp_input_pulse_source_init(&source, sample_base, 1u);
    passed &= recomp_input_pulse_source_sample(NULL, &pad) ? 0 : 1;
    passed &= recomp_input_pulse_source_sample(&source, NULL) ? 0 : 1;
    passed &= expect_u64("null safety count", source.sample_count, 0u);

    /* pulse_poll 0 is rejected into a state that never pulses */
    reset_base();
    recomp_input_pulse_source_init(&source, sample_base, 0u);
    for (unsigned i = 0u; i < 4u; ++i) {
        passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
        passed &= expect_buttons("zero pulse buttons", pad.buttons, 0u);
    }
    passed &= expect_u64("zero pulse poll", source.pulse_poll_count, 0u);
    passed &= expect_u64("zero pulse count", source.sample_count, 4u);

    /* UINT64_MAX pulses only on the max-th successful sample */
    reset_base();
    recomp_input_pulse_source_init(&source, sample_base, UINT64_MAX);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons("max pre-pulse buttons", pad.buttons, 0u);
    source.sample_count = UINT64_MAX - 1u;
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons(
        "max pulse buttons", pad.buttons, RECOMP_INPUT_BUTTON_START);
    passed &= expect_u64("max pulse count", source.sample_count, UINT64_MAX);

    /* two armed polls press START twice with a neutral poll between them */
    reset_base();
    recomp_input_pulse_source_init(&source, sample_base, 2u);
    passed &= recomp_input_pulse_source_add_poll(&source, 4u) ? 1 : 0;
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons("two-poll 1 buttons", pad.buttons, 0u);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons(
        "two-poll 2 buttons", pad.buttons, RECOMP_INPUT_BUTTON_START);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons("two-poll 3 buttons", pad.buttons, 0u);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons(
        "two-poll 4 buttons", pad.buttons, RECOMP_INPUT_BUTTON_START);
    passed &= recomp_input_pulse_source_sample(&source, &pad) ? 1 : 0;
    passed &= expect_buttons("two-poll 5 buttons", pad.buttons, 0u);

    /* arming is bounded and rejects a zero poll */
    recomp_input_pulse_source_init(&source, sample_base, 1u);
    passed &= recomp_input_pulse_source_add_poll(&source, 0u) ? 0 : 1;
    passed &= recomp_input_pulse_source_add_poll(NULL, 2u) ? 0 : 1;
    for (unsigned i = 1u; i < RECOMP_INPUT_PULSE_POLL_CAPACITY; ++i) {
        passed &= recomp_input_pulse_source_add_poll(&source, i + 1u) ? 1 : 0;
    }
    passed &= recomp_input_pulse_source_add_poll(&source, 99u) ? 0 : 1;
    passed &= expect_u64(
        "armed poll count",
        source.pulse_poll_count,
        RECOMP_INPUT_PULSE_POLL_CAPACITY);

    return passed;
}

#ifdef RECOMP_INPUT_PULSE_SOURCE_STANDALONE
int main(void)
{
    return recomp_input_pulse_source_test() ? 0 : 1;
}
#endif
