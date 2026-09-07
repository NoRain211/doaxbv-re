#include "dsound_service_adapter.h"
#include "program_manual.h"
#include "runtime.h"
#include "xbox_memory_layout.h"
#include "audio_output.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Unit tests keep output disconnected from the user's device. */
static uint64_t test_now;
static uint32_t output_bytes;
static uint8_t output_first;
static uint32_t output_calls, output_resets;
static uint32_t output_rate, output_channels, output_bits;
static int16_t output_pcm[40000];
uint64_t recomp_test_dsound_now_ms(void) { return test_now; }
void recomp_audio_output_reset_voice(uint32_t slot) { (void)slot; ++output_resets; }
void recomp_audio_output_submit(uint32_t slot, const uint8_t *pcm,
    uint32_t bytes, uint32_t sample_rate, uint32_t channels,
    uint32_t bits_per_sample, int32_t volume_hundredth_db)
{
    output_bytes = bytes;
    output_first = pcm[0];
    ++output_calls;
    output_rate = sample_rate;
    output_channels = channels;
    output_bits = bits_per_sample;
    if (bytes <= sizeof output_pcm) memcpy(output_pcm, pcm, bytes);
    else output_bytes = 0u;
    (void)slot; (void)volume_hundredth_db;
}

enum {
    TEST_STATIC_BASE = 0x00210000u,
    TEST_STATIC_SIZE = 0x00090000u,
    TEST_CALL_BASE = 0x28000000u,
    TEST_CALL_SIZE = 0x00001000u,
    TEST_HEAP_BASE = 0x27000000u,
    TEST_HEAP_SIZE = 0x00010000u,
    TEST_ENTRY_ESP = TEST_CALL_BASE + 0x100u,
    TEST_OUTPUT = TEST_CALL_BASE + 0x200u,
};

void recomp_test_heap_reset(uint32_t cursor, int fail_after);
unsigned recomp_test_contiguous_allocation_count(void);
void recomp_test_contiguous_allocation_arguments(
    uint32_t *size,
    uint32_t *lowest_address,
    uint32_t *highest_address,
    uint32_t *alignment);

static int expect_u32(
    const char *field,
    uint32_t actual,
    uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "DirectSound service adapter: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

static uint32_t float_bits(float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static void write_argument(uint32_t index, uint32_t value)
{
    *recomp_memory_u32(TEST_ENTRY_ESP + 4u + index * 4u) = value;
}

void recomp_test_dsound_set_data(void)
{
    uint32_t entry = recomp_runtime.registers.esp;
    uint32_t buffer = *recomp_memory_u32(entry + 4u);
    uint32_t data = *recomp_memory_u32(entry + 8u);
    uint32_t size = *recomp_memory_u32(entry + 12u);
    uint32_t settings = *recomp_memory_u32(buffer);

    if (*recomp_memory_u32(settings + 0xb8u) != data ||
        *recomp_memory_u32(settings + 0xbcu) != size) {
        *recomp_memory_u32(settings + 0xb8u) = data;
        *recomp_memory_u32(settings + 0xbcu) = size;
        *recomp_memory_u32(settings + 0xc0u) = 0u;
        *recomp_memory_u32(settings + 0xc4u) = size;
        *recomp_memory_u32(settings + 0xc8u) = 0u;
        *recomp_memory_u32(settings + 0xccu) = size;
    }
    recomp_runtime.registers.eax = RECOMP_DSOUND_OK;
    recomp_runtime.registers.esp = entry + 16u;
}

static uint32_t buffer_test_call(
    uint32_t operation, uint32_t buffer, uint32_t a, uint32_t b, uint32_t c)
{
    write_argument(0u, buffer);
    write_argument(1u, a);
    write_argument(2u, b);
    write_argument(3u, c);
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_dsound_service_lookup_manual(operation)();
    return recomp_runtime.registers.eax;
}

static int test_adpcm_buffer(void)
{
    const uint32_t buffer = TEST_HEAP_BASE + 0x400u;
    const uint32_t settings = TEST_HEAP_BASE + 0x500u;
    const uint32_t data = TEST_STATIC_BASE + 0x30000u;
    uint32_t calls, resets;
    int passed = 1;

    recomp_dsound_service_adapter_reset();
    test_now = 0u;
    *recomp_memory_u32(buffer) = settings;
    *recomp_memory_u32(settings + 0xcu) = 0x04010069u;
    *recomp_memory_u32(settings + 0x10u) = 1000u;
    *recomp_memory_u32(settings + 0x14u) = 36u;
    /* Two independently headed ramps: 100..163, then 1000..1063. */
    recomp_guest_memset(data, 0, 72u);
    *recomp_memory_u16(data) = 100u;
    *recomp_memory_u16(data + 36u) = 1000u;
    recomp_guest_memset(data + 4u, 0x11, 32u);
    recomp_guest_memset(data + 40u, 0x11, 32u);
    passed &= expect_u32("ADPCM set data", buffer_test_call(
        0x001f9e5eu, buffer, data, 72u, 0u), RECOMP_DSOUND_OK);
    passed &= expect_u32("set data ESP", recomp_runtime.registers.esp,
        TEST_ENTRY_ESP + 16u);
    passed &= expect_u32("ADPCM play", buffer_test_call(
        0x001f8fd8u, buffer, 0u, 0u, 0u), RECOMP_DSOUND_OK);
    test_now = 10u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    passed &= expect_u32("ADPCM 10 ms output bytes", output_bytes, 20u);
    passed &= expect_u32("ADPCM output rate", output_rate, 1000u);
    passed &= expect_u32("ADPCM output channels", output_channels, 1u);
    passed &= expect_u32("ADPCM output bits", output_bits, 16u);
    for (unsigned i = 0u; i < 10u; ++i)
        passed &= expect_u32("ADPCM initial samples", output_pcm[i], 100u + i);
    buffer_test_call(0x001f9074u, buffer, TEST_OUTPUT, TEST_OUTPUT + 4u, 0u);
    passed &= expect_u32("ADPCM partial block cursor", *recomp_memory_u32(TEST_OUTPUT), 0u);
    test_now = 64u;
    buffer_test_call(0x001f9074u, buffer, TEST_OUTPUT, TEST_OUTPUT + 4u, 0u);
    passed &= expect_u32("ADPCM second block cursor", *recomp_memory_u32(TEST_OUTPUT), 36u);
    passed &= expect_u32("ADPCM write cursor", *recomp_memory_u32(TEST_OUTPUT + 4u), 36u);

    test_now = 120u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    passed &= expect_u32("ADPCM second block output", output_bytes, 112u);
    passed &= expect_u32("ADPCM second header", output_pcm[0], 1000u);
    test_now = 128u;
    buffer_test_call(0x001f9058u, buffer, TEST_OUTPUT, 0u, 0u);
    passed &= expect_u32("ADPCM one-shot completed", *recomp_memory_u32(TEST_OUTPUT), 0u);
    /* Replay before the output clock's next service tick. */
    buffer_test_call(0x001f8fd8u, buffer, 0u, 0u, 0u);
    test_now = 138u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    passed &= expect_u32("ADPCM replay bytes", output_bytes, 20u);
    passed &= expect_u32("ADPCM replay starts at zero", output_pcm[0], 100u);

    test_now = 158u;
    buffer_test_call(0x001f8ffcu, buffer, 0u, 0u, 0u);
    calls = output_calls;
    test_now = 500u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    passed &= expect_u32("ADPCM stopped output", output_calls, calls);
    buffer_test_call(0x001f8fd8u, buffer, 0u, 0u, 0u);
    test_now = 510u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    passed &= expect_u32("ADPCM resume bytes", output_bytes, 20u);
    passed &= expect_u32("ADPCM paused time excluded", output_pcm[0], 130u);
    passed &= expect_u32("ADPCM seek second block", buffer_test_call(
        0x001f9094u, buffer, 36u, 0u, 0u), RECOMP_DSOUND_OK);
    test_now = 520u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    passed &= expect_u32("ADPCM seek output", output_pcm[0], 1000u);

    buffer_test_call(0x001f9094u, buffer, 36u, 0u, 0u);
    buffer_test_call(0x001f8fd8u, buffer, 0u, 0u, RECOMP_DSOUND_PLAY_LOOPING);
    test_now = 574u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    test_now = 594u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    passed &= expect_u32("ADPCM ring wrap bytes", output_bytes, 40u);
    for (unsigned i = 0u; i < 20u; ++i)
        passed &= expect_u32("ADPCM ring wrap samples", output_pcm[i],
            i < 10u ? 1054u + i : 100u + i - 10u);

    resets = output_resets;
    buffer_test_call(0x001f9e5eu, buffer, data, 72u, 0u);
    passed &= expect_u32("same buffer data keeps voice", output_resets, resets);
    buffer_test_call(0x001f9058u, buffer, TEST_OUTPUT, 0u, 0u);
    passed &= expect_u32("same buffer data keeps playing", *recomp_memory_u32(TEST_OUTPUT), 5u);
    test_now = 604u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    passed &= expect_u32("same buffer data keeps cursor", output_pcm[0], 110u);
    buffer_test_call(0x001f9e5eu, buffer, data, 36u, 0u);
    passed &= expect_u32("resized buffer resets voice", output_resets, resets + 1u);
    buffer_test_call(0x001f9058u, buffer, TEST_OUTPUT, 0u, 0u);
    passed &= expect_u32("resized buffer is stopped", *recomp_memory_u32(TEST_OUTPUT), 0u);
    calls = output_calls;
    test_now = 614u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    passed &= expect_u32("resized buffer stops output", output_calls, calls);
    buffer_test_call(0x001f8fd8u, buffer, 0u, 0u, 0u);
    test_now = 624u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    passed &= expect_u32("resized buffer restarts", output_pcm[0], 100u);
    resets = output_resets;
    buffer_test_call(0x001f9e5eu, buffer, data + 36u, 36u, 0u);
    passed &= expect_u32("new buffer data resets voice", output_resets, resets + 1u);
    buffer_test_call(0x001f8fd8u, buffer, 0u, 0u, 0u);
    test_now = 634u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    passed &= expect_u32("new buffer data output", output_pcm[0], 1000u);
    resets = output_resets;
    calls = output_calls;
    buffer_test_call(0x001f9e5eu, buffer, 0u, 0u, 0u);
    passed &= expect_u32("cleared buffer resets voice", output_resets, resets + 1u);
    test_now = 644u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    passed &= expect_u32("cleared buffer stops output", output_calls, calls);

    /* The observed 12960-byte mono effect has 23040 frames, so at 44100 Hz
       it is still playing at 522 ms and completes at 523 ms. */
    recomp_dsound_service_adapter_reset();
    test_now = 0u;
    *recomp_memory_u32(settings + 0x10u) = 44100u;
    recomp_guest_memset(data, 0, 12960u);
    buffer_test_call(0x001f9e5eu, buffer, data, 12960u, 0u);
    buffer_test_call(0x001f8fd8u, buffer, 0u, 0u, 0u);
    test_now = 522u;
    buffer_test_call(0x001f9058u, buffer, TEST_OUTPUT, 0u, 0u);
    passed &= expect_u32("ADPCM effect at 522 ms", *recomp_memory_u32(TEST_OUTPUT), 1u);
    buffer_test_call(0x001f9074u, buffer, TEST_OUTPUT, 0u, 0u);
    passed &= expect_u32("ADPCM effect final block", *recomp_memory_u32(TEST_OUTPUT), 12924u);
    test_now = 523u;
    buffer_test_call(0x001f9058u, buffer, TEST_OUTPUT, 0u, 0u);
    passed &= expect_u32("ADPCM effect at 523 ms", *recomp_memory_u32(TEST_OUTPUT), 0u);

    /* Two stereo intro blocks, then a 64-frame tail. Each channel has a
       distinct ramp so a wrap to the intro or a wrong lane is observable. */
    recomp_dsound_service_adapter_reset();
    test_now = 0u;
    *recomp_memory_u32(settings + 0xcu) = 0x04020069u;
    *recomp_memory_u32(settings + 0x10u) = 1000u;
    *recomp_memory_u32(settings + 0x14u) = 72u;
    recomp_guest_memset(data, 0, 216u);
    for (unsigned block = 0u; block < 3u; ++block) {
        uint16_t start = block == 0u ? 100u : block == 1u ? 500u : 1000u;
        *recomp_memory_u16(data + block * 72u) = start;
        *recomp_memory_u16(data + block * 72u + 4u) = start + 100u;
        recomp_guest_memset(data + block * 72u + 8u, 0x11, 64u);
    }
    buffer_test_call(0x001f9e5eu, buffer, data, 216u, 0u);
    *recomp_memory_u32(settings + 0xc8u) = 144u;
    *recomp_memory_u32(settings + 0xccu) = 72u;
    passed &= expect_u32("ADPCM tail play", buffer_test_call(
        0x001f8fd8u, buffer, 0u, 0u, RECOMP_DSOUND_PLAY_LOOPING), RECOMP_DSOUND_OK);
    for (unsigned tick = 1u; tick <= 3u; ++tick) {
        unsigned frames = tick == 3u ? 100u : 90u;
        test_now = tick == 3u ? 280u : tick * 90u;
        buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
        passed &= expect_u32("ADPCM tail output bytes", output_bytes, frames * 4u);
        for (unsigned i = 0u; i < frames; ++i) {
            unsigned frame = tick == 1u ? i : tick == 2u ? 90u + i : 180u + i;
            unsigned expected = frame < 64u ? 100u + frame :
                frame < 128u ? 500u + frame - 64u : 1000u + (frame - 128u) % 64u;
            passed &= expect_u32("ADPCM intro/tail left", output_pcm[i * 2u], expected);
            passed &= expect_u32("ADPCM intro/tail right", output_pcm[i * 2u + 1u], expected + 100u);
        }
    }
    buffer_test_call(0x001f9074u, buffer, TEST_OUTPUT, 0u, 0u);
    passed &= expect_u32("ADPCM tail compressed cursor", *recomp_memory_u32(TEST_OUTPUT), 144u);
    buffer_test_call(0x001f8fd8u, buffer, 0u, 0u, RECOMP_DSOUND_PLAY_FROMSTART);
    test_now = 370u;
    buffer_test_call(0x001f90e0u, 0u, 0u, 0u, 0u);
    passed &= expect_u32("ADPCM one-shot keeps intro", output_pcm[0], 100u);
    test_now = 472u;
    buffer_test_call(0x001f9058u, buffer, TEST_OUTPUT, 0u, 0u);
    passed &= expect_u32("ADPCM one-shot ignores tail loop", *recomp_memory_u32(TEST_OUTPUT), 0u);
    recomp_dsound_service_adapter_reset();
    test_now = 0u;
    return passed;
}

static int expect_lookup(uint32_t address)
{
    RecompFunction adapter = recomp_dsound_service_lookup_manual(address);

    if (adapter != NULL && recomp_lookup_manual(address) == adapter) {
        return 1;
    }
    fprintf(
        stderr,
        "DirectSound service adapter: lookup failed for 0x%08x\n",
        address);
    return 0;
}

int recomp_dsound_service_adapter_test(void)
{
    static uint8_t static_memory[TEST_STATIC_SIZE];
    static uint8_t call_memory[TEST_CALL_SIZE];
    static uint8_t heap_memory[TEST_HEAP_SIZE];
    const RecompMemoryRegion regions[] = {
        {
            .address = TEST_STATIC_BASE,
            .size = sizeof static_memory,
            .data = static_memory,
        },
        {
            .address = TEST_CALL_BASE,
            .size = sizeof call_memory,
            .data = call_memory,
        },
        {
            .address = TEST_HEAP_BASE,
            .size = sizeof heap_memory,
            .data = heap_memory,
        },
    };
    const RecompDsoundServiceModel *model;
    RecompFunction adapter;
    int passed = 1;

    memset(static_memory, 0, sizeof static_memory);
    memset(call_memory, 0, sizeof call_memory);
    memset(heap_memory, 0xa5, sizeof heap_memory);
    recomp_runtime_init(regions, 3u, NULL, 0u, NULL, 0u);
    recomp_test_heap_reset(TEST_HEAP_BASE, -1);
    recomp_dsound_service_adapter_reset();
    model = recomp_dsound_service_adapter_model();
    passed &= expect_u32("reset work count", model->work_count, 0u);
    passed &= expect_u32("reset commit count", model->commit_count, 0u);
    passed &= expect_u32(
        "reset mix-bin headroom count", model->mix_bin_headroom_count, 0u);
    passed &= expect_u32(
        "reset voice state table", model->voice_state_table, 0u);
    passed &= expect_lookup(0x001f90e0u);
    passed &= expect_lookup(0x001f8f21u);
    passed &= expect_lookup(0x001f8f48u);
    passed &= expect_lookup(0x001fa27cu);
    passed &= expect_lookup(0x001f974fu);
    passed &= expect_lookup(0x001f9dd4u);
    passed &= expect_lookup(0x001f9e09u);
    if (recomp_dsound_service_lookup_manual(0x001fa27bu) != NULL ||
        recomp_dsound_service_lookup_manual(0x001fa27du) != NULL ||
        recomp_dsound_service_lookup_manual(0x001f8f20u) != NULL ||
        recomp_dsound_service_lookup_manual(0x001f8f22u) != NULL ||
        recomp_dsound_service_lookup_manual(0x001f8f47u) != NULL ||
        recomp_dsound_service_lookup_manual(0x001f8f49u) != NULL ||
        recomp_dsound_service_lookup_manual(0x001f90dfu) != NULL ||
        recomp_dsound_service_lookup_manual(0x001f9e3eu) != NULL) {
        fprintf(stderr, "DirectSound service adapter: lookup was not exact\n");
        return 0;
    }

    write_argument(0u, 0u);
    write_argument(1u, TEST_OUTPUT);
    write_argument(2u, 0u);
    *recomp_memory_u32(TEST_OUTPUT) = 0xa5a5a5a5u;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    adapter = recomp_dsound_service_lookup_manual(0x001fa27cu);
    adapter();
    passed &= expect_u32(
        "create ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 16u);
    passed &= expect_u32(
        "create HRESULT", recomp_runtime.registers.eax, RECOMP_DSOUND_OK);
    passed &= expect_u32(
        "create output", *recomp_memory_u32(TEST_OUTPUT),
        TEST_HEAP_BASE + 8u);
    passed &= expect_u32(
        "manager vtable", *recomp_memory_u32(TEST_HEAP_BASE), 0x00239decu);
    passed &= expect_u32(
        "manager references", *recomp_memory_u32(TEST_HEAP_BASE + 4u), 2u);
    passed &= expect_u32(
        "manager device", *recomp_memory_u32(TEST_HEAP_BASE + 8u),
        TEST_HEAP_BASE + 0x30u);
    passed &= expect_u32(
        "manager list head", *recomp_memory_u32(TEST_HEAP_BASE + 0x10u),
        TEST_HEAP_BASE + 0x10u);
    passed &= expect_u32(
        "manager list tail", *recomp_memory_u32(TEST_HEAP_BASE + 0x14u),
        TEST_HEAP_BASE + 0x10u);
    passed &= expect_u32(
        "manager global", *recomp_memory_u32(0x00214708u), TEST_HEAP_BASE);
    passed &= expect_u32("model manager", model->manager, TEST_HEAP_BASE);
    passed &= expect_u32(
        "device vtable", *recomp_memory_u32(TEST_HEAP_BASE + 0x30u),
        0x00239e1cu);
    passed &= expect_u32(
        "device references", *recomp_memory_u32(TEST_HEAP_BASE + 0x34u),
        1u);
    passed &= expect_u32(
        "model device", model->device, TEST_HEAP_BASE + 0x30u);

    {
        uint32_t apu = TEST_HEAP_BASE + 0xe0u;
        uint32_t pool = apu + 0x300u;

        passed &= expect_u32("model apu", model->apu, apu);
        passed &= expect_u32(
            "manager apu", *recomp_memory_u32(TEST_HEAP_BASE + 0x0cu), apu);
        passed &= expect_u32(
            "apu vtable", *recomp_memory_u32(apu), 0x00239e44u);
        passed &= expect_u32(
            "apu references", *recomp_memory_u32(apu + 4u), 1u);
        passed &= expect_u32(
            "apu inner vtable", *recomp_memory_u32(apu + 8u), 0x00239e40u);
        passed &= expect_u32(
            "apu device", *recomp_memory_u32(apu + 0x0cu),
            TEST_HEAP_BASE + 0x30u);
        passed &= expect_u32(
            "apu page pool vtable", *recomp_memory_u32(pool), 0x00239e98u);
        /* The empty self-linked block list is what stops the CMcpxBuffer_Play
           walk that previously ran off into unmapped memory. */
        passed &= expect_u32(
            "apu page pool block list", *recomp_memory_u32(pool + 4u),
            pool + 4u);
        passed &= expect_u32(
            "apu page pool block list back", *recomp_memory_u32(pool + 8u),
            pool + 4u);
        passed &= expect_u32(
            "apu page pool second list", *recomp_memory_u32(pool + 0x0cu),
            pool + 0x0cu);
        passed &= expect_u32(
            "apu page pool tag", *recomp_memory_u32(pool + 0x1cu),
            0x00214074u);
        /* sub_001FB4C2 leaves the largest-free-block cache null, so an
           allocation request fails instead of mapping pages. */
        passed &= expect_u32(
            "apu page pool cache", *recomp_memory_u32(pool + 0x18u), 0u);
        passed &= expect_u32(
            "apu tail list first", *recomp_memory_u32(apu + 0x728u),
            apu + 0x728u);
        passed &= expect_u32(
            "apu tail list last", *recomp_memory_u32(apu + 0x750u),
            apu + 0x750u);
        passed &= expect_u32(
            "apu tail list last back", *recomp_memory_u32(apu + 0x754u),
            apu + 0x750u);
        passed &= expect_u32(
            "apu counter a", *recomp_memory_u32(0x0021406cu), 0xc0u);
        passed &= expect_u32(
            "apu counter b", *recomp_memory_u32(0x00214070u), 0x40u);
    }

    {
        uint32_t voices = TEST_HEAP_BASE + 0x8000u;
        uint32_t voice_index;
        uint32_t offset;
        uint32_t allocation_size;
        uint32_t allocation_lowest;
        uint32_t allocation_highest;
        uint32_t allocation_alignment;

        recomp_test_contiguous_allocation_arguments(
            &allocation_size,
            &allocation_lowest,
            &allocation_highest,
            &allocation_alignment);

        passed &= expect_u32(
            "model voice state table", model->voice_state_table, voices);
        passed &= expect_u32(
            "voice state global", *recomp_memory_u32(0x002147e8u), voices);
        passed &= expect_u32(
            "voice state alignment",
            voices & (RECOMP_DSOUND_VOICE_STATE_TABLE_SIZE - 1u),
            0u);
        passed &= expect_u32(
            "voice state size constant",
            RECOMP_DSOUND_VOICE_STATE_SIZE,
            0x80u);
        passed &= expect_u32(
            "voice state count constant",
            RECOMP_DSOUND_VOICE_STATE_COUNT,
            0x100u);
        passed &= expect_u32(
            "voice table size constant",
            RECOMP_DSOUND_VOICE_STATE_TABLE_SIZE,
            0x8000u);
        passed &= expect_u32(
            "contiguous allocation count",
            recomp_test_contiguous_allocation_count(),
            1u);
        passed &= expect_u32(
            "contiguous allocation size", allocation_size, 0x8000u);
        passed &= expect_u32(
            "contiguous allocation lowest", allocation_lowest, 0u);
        passed &= expect_u32(
            "contiguous allocation highest", allocation_highest, UINT32_MAX);
        passed &= expect_u32(
            "contiguous allocation alignment", allocation_alignment, 0x8000u);
        for (voice_index = 0u;
             voice_index < RECOMP_DSOUND_VOICE_STATE_COUNT;
             ++voice_index) {
            uint32_t record =
                voices + voice_index * RECOMP_DSOUND_VOICE_STATE_SIZE;

            for (offset = 0u;
                 offset < RECOMP_DSOUND_VOICE_STATE_SIZE;
                 offset += 4u) {
                uint32_t expected =
                    offset == RECOMP_DSOUND_VOICE_STATE_INDEX_OFFSET
                    ? voice_index
                    : 0u;

                passed &= expect_u32(
                    "voice state dword",
                    *recomp_memory_u32(record + offset),
                    expected);
            }
        }
    }

    write_argument(0u, TEST_HEAP_BASE + 8u);
    write_argument(1u, TEST_CALL_BASE + 0x300u);
    write_argument(2u, 18812u);
    write_argument(3u, TEST_CALL_BASE + 0x380u);
    write_argument(4u, TEST_OUTPUT);
    *recomp_memory_u32(TEST_OUTPUT) = 0xa5a5a5a5u;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    adapter = recomp_dsound_service_lookup_manual(0x001f8f21u);
    adapter();
    passed &= expect_u32(
        "download effects ESP", recomp_runtime.registers.esp,
        TEST_ENTRY_ESP + 24u);
    passed &= expect_u32(
        "download effects HRESULT", recomp_runtime.registers.eax,
        RECOMP_DSOUND_OK);
    passed &= expect_u32(
        "download effects descriptor", *recomp_memory_u32(TEST_OUTPUT), 0u);
    passed &= expect_u32(
        "download effects buffer", model->effects_image_buffer,
        TEST_CALL_BASE + 0x300u);
    passed &= expect_u32(
        "download effects size", model->effects_image_size, 18812u);
    passed &= expect_u32(
        "download effects location", model->effects_image_location,
        TEST_CALL_BASE + 0x380u);
    passed &= expect_u32(
        "download effects count", model->effects_image_download_count, 1u);

    write_argument(0u, TEST_HEAP_BASE + 8u);
    write_argument(1u, 10u);
    write_argument(2u, 0u);
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xffffffffu;
    adapter = recomp_dsound_service_lookup_manual(0x001f8f48u);
    adapter();
    passed &= expect_u32(
        "mix-bin headroom ESP", recomp_runtime.registers.esp,
        TEST_ENTRY_ESP + 16u);
    passed &= expect_u32(
        "mix-bin headroom HRESULT", recomp_runtime.registers.eax,
        RECOMP_DSOUND_OK);
    passed &= expect_u32(
        "mix-bin headroom bin", model->mix_bin, 10u);
    passed &= expect_u32(
        "mix-bin headroom value", model->mix_bin_headroom, 0u);
    passed &= expect_u32(
        "mix-bin headroom count", model->mix_bin_headroom_count, 1u);

    recomp_dsound_service_adapter_reset();
    recomp_test_heap_reset(TEST_HEAP_BASE + 0x100u, 1);
    adapter = recomp_dsound_service_lookup_manual(0x001fa27cu);
    write_argument(1u, TEST_OUTPUT);
    *recomp_memory_u32(TEST_OUTPUT) = 0xa5a5a5a5u;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    adapter();
    passed &= expect_u32(
        "device allocation failure HRESULT", recomp_runtime.registers.eax,
        RECOMP_DSOUND_OUT_OF_MEMORY);
    passed &= expect_u32(
        "device allocation failure output",
        *recomp_memory_u32(TEST_OUTPUT), 0u);
    passed &= expect_u32(
        "device allocation rollback", xbox_HeapCheckpoint(),
        TEST_HEAP_BASE + 0x100u);
    passed &= expect_u32(
        "device failure contiguous allocations",
        recomp_test_contiguous_allocation_count(),
        0u);

    recomp_dsound_service_adapter_reset();
    recomp_test_heap_reset(TEST_HEAP_BASE + 0x400u, 2);
    write_argument(1u, TEST_OUTPUT);
    *recomp_memory_u32(TEST_OUTPUT) = 0xa5a5a5a5u;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    adapter();
    passed &= expect_u32(
        "apu allocation failure HRESULT", recomp_runtime.registers.eax,
        RECOMP_DSOUND_OUT_OF_MEMORY);
    passed &= expect_u32(
        "apu allocation failure output",
        *recomp_memory_u32(TEST_OUTPUT), 0u);
    passed &= expect_u32(
        "apu allocation rollback", xbox_HeapCheckpoint(),
        TEST_HEAP_BASE + 0x400u);
    passed &= expect_u32(
        "apu failure contiguous allocations",
        recomp_test_contiguous_allocation_count(),
        0u);

    recomp_dsound_service_adapter_reset();
    recomp_test_heap_reset(TEST_HEAP_BASE + 0x400u, 3);
    write_argument(1u, TEST_OUTPUT);
    *recomp_memory_u32(TEST_OUTPUT) = 0xa5a5a5a5u;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    adapter();
    passed &= expect_u32(
        "voice-state allocation failure HRESULT", recomp_runtime.registers.eax,
        RECOMP_DSOUND_OUT_OF_MEMORY);
    passed &= expect_u32(
        "voice-state allocation failure output",
        *recomp_memory_u32(TEST_OUTPUT), 0u);
    passed &= expect_u32(
        "voice-state allocation rollback", xbox_HeapCheckpoint(),
        TEST_HEAP_BASE + 0x400u);
    passed &= expect_u32(
        "voice-state failure contiguous allocations",
        recomp_test_contiguous_allocation_count(),
        1u);

    recomp_dsound_service_adapter_reset();
    recomp_test_heap_reset(TEST_HEAP_BASE + 0x200u, 0);
    *recomp_memory_u32(TEST_OUTPUT) = 0xa5a5a5a5u;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    adapter();
    passed &= expect_u32(
        "manager allocation failure HRESULT", recomp_runtime.registers.eax,
        RECOMP_DSOUND_OUT_OF_MEMORY);
    passed &= expect_u32(
        "manager allocation failure output",
        *recomp_memory_u32(TEST_OUTPUT), 0u);
    passed &= expect_u32(
        "manager allocation failure heap", xbox_HeapCheckpoint(),
        TEST_HEAP_BASE + 0x200u);

    recomp_dsound_service_adapter_reset();
    recomp_test_heap_reset(TEST_HEAP_BASE + 0x300u, -1);
    write_argument(1u, 0u);
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    adapter();
    passed &= expect_u32(
        "null output HRESULT", recomp_runtime.registers.eax,
        RECOMP_DSOUND_POINTER_ERROR);
    passed &= expect_u32(
        "null output ESP", recomp_runtime.registers.esp,
        TEST_ENTRY_ESP + 16u);

    recomp_dsound_service_adapter_reset();
    recomp_test_heap_reset(TEST_HEAP_BASE, -1);

    write_argument(0u, 8u);
    write_argument(1u, float_bits(1.25f));
    write_argument(2u, float_bits(-2.5f));
    write_argument(3u, float_bits(3.75f));
    write_argument(4u, 1u);
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xffffffffu;
    adapter = recomp_dsound_service_lookup_manual(0x001f9dd4u);
    adapter();
    passed &= expect_u32(
        "position ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 24u);
    passed &= expect_u32("position HRESULT", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32(
        "position x", float_bits(model->listener_position.x),
        float_bits(1.25f));
    passed &= expect_u32(
        "position y", float_bits(model->listener_position.y),
        float_bits(-2.5f));
    passed &= expect_u32(
        "position z", float_bits(model->listener_position.z),
        float_bits(3.75f));
    passed &= expect_u32("position apply", model->position_apply, 1u);

    write_argument(1u, float_bits(-4.0f));
    write_argument(2u, float_bits(5.5f));
    write_argument(3u, float_bits(6.0f));
    write_argument(4u, 2u);
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xffffffffu;
    adapter = recomp_dsound_service_lookup_manual(0x001f9e09u);
    adapter();
    passed &= expect_u32(
        "velocity ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 24u);
    passed &= expect_u32("velocity HRESULT", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32(
        "velocity x", float_bits(model->listener_velocity.x),
        float_bits(-4.0f));
    passed &= expect_u32(
        "velocity y", float_bits(model->listener_velocity.y),
        float_bits(5.5f));
    passed &= expect_u32(
        "velocity z", float_bits(model->listener_velocity.z),
        float_bits(6.0f));
    passed &= expect_u32("velocity apply", model->velocity_apply, 2u);

    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xffffffffu;
    adapter = recomp_dsound_service_lookup_manual(0x001f974fu);
    adapter();
    passed &= expect_u32(
        "commit ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 8u);
    passed &= expect_u32("commit HRESULT", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32("commit count", model->commit_count, 1u);

    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xa5a5a5a5u;
    adapter = recomp_dsound_service_lookup_manual(0x001f90e0u);
    adapter();
    passed &= expect_u32(
        "work ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 4u);
    passed &= expect_u32(
        "work preserved EAX", recomp_runtime.registers.eax, 0xa5a5a5a5u);
    passed &= expect_u32("work count", model->work_count, 1u);

    {
        RecompDsoundBufferModel clock;
        recomp_dsound_buffer_configure(&clock, 65536u, 44100u, 4u, 0u);
        recomp_dsound_buffer_play(&clock, 1u, 0u);
        for (uint64_t ms = 1u; ms <= 1000u; ++ms) {
            recomp_dsound_buffer_cursor(&clock, ms);
        }
        passed &= expect_u32("fractional PCM ring cursor", clock.cursor_bytes, 45328u);
        recomp_dsound_buffer_stop(&clock, 1000u);
        recomp_dsound_buffer_play(&clock, 1u, 10000u);
        passed &= expect_u32("paused time excluded",
            recomp_dsound_buffer_cursor(&clock, 10100u), 62968u);
    }

    {
        RecompDsoundBufferModel output;
        uint32_t offset = 0u;
        recomp_dsound_buffer_configure(&output, 400u, 1000u, 4u, 0u);
        recomp_dsound_buffer_play(&output, 1u, 0u);
        passed &= expect_u32("PCM consumed bytes",
            recomp_dsound_buffer_consume(&output, 90u, &offset), 360u);
        passed &= expect_u32("PCM initial offset", offset, 0u);
        passed &= expect_u32("PCM wrap bytes",
            recomp_dsound_buffer_consume(&output, 110u, &offset), 80u);
        passed &= expect_u32("PCM wrap offset", offset, 360u);
        passed &= expect_u32("PCM wrapped cursor", output.cursor_bytes, 40u);
        passed &= expect_u32("PCM stalled output discarded",
            recomp_dsound_buffer_consume(&output, 500u, &offset), 0u);
        passed &= expect_u32("PCM stalled clock caught up", output.cursor_bytes, 0u);
        recomp_dsound_buffer_play(&output, 2u, 500u);
        recomp_dsound_buffer_consume(&output, 590u, &offset);
        passed &= expect_u32("PCM one-shot tail",
            recomp_dsound_buffer_consume(&output, 610u, &offset), 40u);
        passed &= expect_u32("PCM tail offset", offset, 360u);
        passed &= expect_u32("PCM one-shot stops", output.playing, 0u);
        passed &= expect_u32("PCM stopped output",
            recomp_dsound_buffer_consume(&output, 620u, &offset), 0u);
        recomp_dsound_buffer_configure(&output, 176400u, 44100u, 4u, 0u);
        recomp_dsound_buffer_play(&output, 1u, 0u);
        uint32_t bytes = 0u;
        for (uint64_t ms = 1u; ms <= 1000u; ++ms) {
            bytes += recomp_dsound_buffer_consume(&output, ms, &offset);
        }
        passed &= expect_u32("PCM fractional output", bytes, 176400u);

        /* A short tail can wrap several times in one <=100 ms submission. */
        recomp_dsound_buffer_configure(&output, 80u, 1000u, 2u, 0u);
        output.loop_start_bytes = 48u;
        recomp_dsound_buffer_play(&output, RECOMP_DSOUND_PLAY_LOOPING, 0u);
        passed &= expect_u32("tail intro bytes",
            recomp_dsound_buffer_consume(&output, 30u, &offset), 60u);
        passed &= expect_u32("tail intro offset", offset, 0u);
        passed &= expect_u32("tail multiple wrap bytes",
            recomp_dsound_buffer_consume(&output, 130u, &offset), 200u);
        passed &= expect_u32("tail multiple wrap offset", offset, 60u);
        passed &= expect_u32("tail multiple wrap cursor", output.cursor_bytes, 68u);
        recomp_dsound_buffer_play(&output, RECOMP_DSOUND_PLAY_FROMSTART, 130u);
        passed &= expect_u32("tail one-shot intro",
            recomp_dsound_buffer_consume(&output, 160u, &offset), 60u);
        passed &= expect_u32("tail one-shot final bytes",
            recomp_dsound_buffer_consume(&output, 190u, &offset), 20u);
        passed &= expect_u32("tail one-shot stops", output.playing, 0u);

        recomp_dsound_buffer_configure(&output, 80u, 11025u, 2u, 0u);
        output.loop_start_bytes = 48u;
        recomp_dsound_buffer_play(&output, RECOMP_DSOUND_PLAY_LOOPING, 0u);
        bytes = 0u;
        for (uint64_t ms = 1u; ms <= 1000u; ++ms)
            bytes += recomp_dsound_buffer_consume(&output, ms, &offset);
        passed &= expect_u32("tail fractional output", bytes, 22050u);
        passed &= expect_u32("tail fractional cursor", output.cursor_bytes, 66u);
        passed &= expect_u32("tail next fractional cursor",
            recomp_dsound_buffer_cursor(&output, 1001u), 56u);
        passed &= expect_u32("tail fractional remainder", output.frame_remainder, 25u);
        recomp_dsound_buffer_configure(&output, 80u, 11025u, 2u, 0u);
        output.loop_start_bytes = 48u;
        recomp_dsound_buffer_play(&output, RECOMP_DSOUND_PLAY_LOOPING, 0u);
        passed &= expect_u32("tail huge elapsed cursor",
            recomp_dsound_buffer_cursor(&output, UINT64_MAX), 54u);
        passed &= expect_u32("tail huge elapsed remainder", output.frame_remainder, 375u);
    }

    /* A PCM buffer whose sound worker polls a byte cursor before playback. */
    {
        uint32_t buffer = TEST_HEAP_BASE + 0x400u;
        uint32_t settings = TEST_HEAP_BASE + 0x500u;
        *recomp_memory_u32(buffer) = settings;
        *recomp_memory_u32(settings + 0xcu) = 0x10020001u;
        *recomp_memory_u32(settings + 0x10u) = 44100u;
        *recomp_memory_u32(settings + 0x14u) = 4u;
        *recomp_memory_u32(settings + 0xbcu) = 65536u;
        *recomp_memory_u32(settings + 0xc0u) = 0u;
        *recomp_memory_u32(settings + 0xc4u) = 65536u;
        *recomp_memory_u32(settings + 0xc8u) = 0u;
        *recomp_memory_u32(settings + 0xccu) = 65536u;
        write_argument(0u, buffer);
        write_argument(1u, 44100u);
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        recomp_dsound_service_lookup_manual(0x001f9767u)();
        passed &= expect_u32("buffer frequency HRESULT", recomp_runtime.registers.eax, 0u);
        write_argument(1u, 32764u);
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        recomp_dsound_service_lookup_manual(0x001f9094u)();
        passed &= expect_u32("buffer seek HRESULT", recomp_runtime.registers.eax, 0u);
        write_argument(1u, TEST_OUTPUT);
        write_argument(2u, 0u);
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        recomp_dsound_service_lookup_manual(0x001f9074u)();
        passed &= expect_u32("stopped byte cursor", *recomp_memory_u32(TEST_OUTPUT), 32764u);
        passed &= expect_u32("cursor ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 16u);
        write_argument(1u, 0u);
        write_argument(3u, 1u);
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        recomp_dsound_service_lookup_manual(0x001f8fd8u)();
        passed &= expect_u32("buffer play HRESULT", recomp_runtime.registers.eax, 0u);
        passed &= expect_u32("play ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 20u);
        write_argument(1u, TEST_OUTPUT);
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        recomp_dsound_service_lookup_manual(0x001f9058u)();
        passed &= expect_u32("looping buffer status", *recomp_memory_u32(TEST_OUTPUT), 5u);
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        recomp_dsound_service_lookup_manual(0x001f8ffcu)();
        passed &= expect_u32("stop ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 8u);
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        recomp_dsound_service_lookup_manual(0x001f9058u)();
        passed &= expect_u32("stopped buffer status", *recomp_memory_u32(TEST_OUTPUT), 0u);
        passed &= expect_lookup(0x001f84b0u);
    }

    /* Replay a completed one-shot before the next 10 ms output service tick. */
    {
        recomp_dsound_service_adapter_reset();
        uint32_t buffer = TEST_HEAP_BASE + 0x400u;
        uint32_t settings = TEST_HEAP_BASE + 0x500u;
        uint32_t data = TEST_STATIC_BASE + 0x30000u;
        *recomp_memory_u32(settings + 0xcu) = 0x10020001u;
        *recomp_memory_u32(settings + 0x10u) = 1000u;
        *recomp_memory_u32(settings + 0xb8u) = data;
        *recomp_memory_u32(settings + 0xbcu) = 380u;
        *recomp_memory_u32(settings + 0xc4u) = 380u;
        *recomp_memory_u32(settings + 0xccu) = 380u;
        recomp_guest_memset(data, 0, 380u);
        *recomp_memory_i8(data) = 42;
        write_argument(0u, buffer);
        write_argument(3u, 0u);
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        recomp_dsound_service_lookup_manual(0x001f8fd8u)();
        test_now = 90u;
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        recomp_dsound_service_lookup_manual(0x001f90e0u)();
        passed &= expect_u32("PCM first one-shot output", output_bytes, 360u);
        test_now = 96u;
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        recomp_dsound_service_lookup_manual(0x001f8fd8u)();
        test_now = 106u;
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        recomp_dsound_service_lookup_manual(0x001f90e0u)();
        passed &= expect_u32("PCM replay output", output_bytes, 40u);
        passed &= expect_u32("PCM replay begins at zero", output_first, 42u);
        recomp_dsound_service_adapter_reset();
        test_now = 0u;
    }

    passed &= test_adpcm_buffer();
    return passed;
}
