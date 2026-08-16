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
    RecompInputPulseSource source;
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
