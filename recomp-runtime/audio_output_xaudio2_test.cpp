#include "audio_output.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <xaudio2.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

/* Substitute only the API calls used by the backend; this test cannot open an
   audio device. Keep checks active in release builds too. */
static void check(bool condition)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL audio output backend\n");
        std::abort();
    }
}

struct FakeSource {
    bool destroyed = false;
    bool started = false;
    float volume = 1.0f;
    std::vector<XAUDIO2_BUFFER> queued;

    void DestroyVoice() { check(!destroyed); destroyed = true; queued.clear(); }
    HRESULT Start() { started = true; return S_OK; }
    void GetState(XAUDIO2_VOICE_STATE *state, UINT32 flags);
    HRESULT SetVolume(float gain) { volume = gain; return S_OK; }
    HRESULT SubmitSourceBuffer(const XAUDIO2_BUFFER *buffer)
    {
        check(started && !destroyed);
        queued.push_back(*buffer);
        return S_OK;
    }
};

static std::vector<FakeSource *> fake_sources;
static FakeSource *current_source;
static std::map<void *, FakeSource *> allocations;
static unsigned com_balance, create_calls, release_calls;
static bool create_unavailable, master_unavailable;
static const char *fake_gain;
static unsigned gain_reads;

void FakeSource::GetState(XAUDIO2_VOICE_STATE *state, UINT32 flags)
{
    check(!destroyed && flags == XAUDIO2_VOICE_NOSAMPLESPLAYED);
    state->BuffersQueued = static_cast<UINT32>(queued.size());
    current_source = this;
}

struct FakeMaster {
    float volume = 1.0f;
    HRESULT SetVolume(float gain) { volume = gain; return S_OK; }
    void DestroyVoice()
    {
        for (const auto *source : fake_sources) check(source->destroyed);
    }
};

struct FakeEngine {
    IXAudio2EngineCallback *callback = nullptr;
    FakeMaster master;
    HRESULT RegisterForCallbacks(IXAudio2EngineCallback *value)
    {
        callback = value;
        return S_OK;
    }
    void UnregisterForCallbacks(IXAudio2EngineCallback *value)
    {
        check(value == callback);
        callback = nullptr;
    }
    HRESULT CreateMasteringVoice(FakeMaster **out)
    {
        if (master_unavailable) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        *out = &master;
        return S_OK;
    }
    HRESULT CreateSourceVoice(FakeSource **out, const WAVEFORMATEX *format,
        UINT32 flags, float ratio)
    {
        check(format->wFormatTag == WAVE_FORMAT_PCM &&
            flags == XAUDIO2_VOICE_NOPITCH && ratio == 1.0f);
        *out = new FakeSource;
        fake_sources.push_back(*out);
        return S_OK;
    }
    void Release() { check(callback == nullptr); ++release_calls; }
};

static FakeEngine fake_engine;

static HRESULT fakeCreate(FakeEngine **out, UINT32 flags, XAUDIO2_PROCESSOR cpu)
{
    check(flags == 0 && cpu == XAUDIO2_DEFAULT_PROCESSOR);
    ++create_calls;
    if (create_unavailable) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    *out = &fake_engine;
    return S_OK;
}

static HRESULT fakeComInitialize(void *, DWORD flags)
{
    check(flags == COINIT_MULTITHREADED);
    ++com_balance;
    return S_OK;
}

static void fakeComUninitialize() { check(com_balance == 1); --com_balance; }

/* Track frees separately from XAudio2 so a premature release fails even when
   the allocator leaves the old PCM bytes intact. */
namespace test_std {
using std::atomic;
using std::fprintf;
using std::isfinite;
using std::memcpy;
using std::memory_order_relaxed;
using std::pow;
using std::strtod;

const char *getenv(const char *name)
{
    check(std::strcmp(name, "RECOMP_AUDIO_GAIN") == 0);
    ++gain_reads;
    return fake_gain;
}

void *realloc(void *old, size_t size)
{
    check(current_source && !current_source->destroyed);
    for (const auto &buffer : current_source->queued)
        check(buffer.pAudioData != old);
    void *data = std::realloc(old, size);
    if (data) {
        allocations.erase(old);
        allocations[data] = current_source;
    }
    return data;
}

void free(void *data)
{
    if (!data) return;
    auto allocation = allocations.find(data);
    check(allocation != allocations.end() && allocation->second->destroyed);
    allocations.erase(allocation);
    std::free(data);
}
}

#define IXAudio2 FakeEngine
#define IXAudio2MasteringVoice FakeMaster
#define IXAudio2SourceVoice FakeSource
#define XAudio2Create fakeCreate
#define CoInitializeEx fakeComInitialize
#define CoUninitialize fakeComUninitialize
#define std test_std
#include "audio_output_xaudio2.cpp"
#undef std
#undef CoUninitialize
#undef CoInitializeEx
#undef XAudio2Create
#undef IXAudio2SourceVoice
#undef IXAudio2MasteringVoice
#undef IXAudio2

int main()
{
    uint8_t pcm[8] = {1, 0, 2, 0, 3, 0, 4, 0};
    const char *muted_values[] = {
        nullptr, "0", "", "garbage", "nan", "inf", "-0.1", "1.01", "1x"};
    for (const char *value : muted_values) {
        attempted = false;
        fake_gain = value;
        const unsigned reads = gain_reads;
        recomp_audio_output_submit(0, pcm, sizeof pcm, 8000, 1, 16, 0);
        recomp_audio_output_submit(0, pcm, sizeof pcm, 8000, 1, 16, 0);
        check(!engine && create_calls == 0 && com_balance == 0 &&
            gain_reads == reads + 1);
    }
    attempted = false;
    fake_gain = "1";
    create_unavailable = true;
    recomp_audio_output_initialize();
    recomp_audio_output_submit(0, pcm, sizeof pcm, 8000, 1, 16, 0);
    check(!engine && create_calls == 1 && com_balance == 0);
    recomp_audio_output_shutdown();
    recomp_audio_output_shutdown();
    check(release_calls == 0);

    /* Begin a separate process lifecycle inside this test translation unit. */
    attempted = summary_printed = false;
    create_unavailable = false;
    master_unavailable = true;
    recomp_audio_output_initialize();
    recomp_audio_output_submit(0, pcm, sizeof pcm, 8000, 1, 16, 0);
    check(!engine && !master && !fake_engine.callback && !callback_registered &&
        create_calls == 2 && release_calls == 1 && com_balance == 0 &&
        fake_sources.empty() && allocations.empty() && submitted_buffers == 0);
    recomp_audio_output_shutdown();
    recomp_audio_output_shutdown();
    check(release_calls == 1);

    attempted = summary_printed = false;
    master_unavailable = false;
    fake_gain = "0.02";
    recomp_audio_output_submit(0, pcm, sizeof pcm, 8000, 1, 16, 1000);
    FakeSource *source = voices[0].source;
    check(source && source->queued.size() == 1 && source->volume == 1.0f);
    check(std::fabs(fake_engine.master.volume - 0.02f) < 0.00001f);
    const uint8_t *copy = source->queued[0].pAudioData;
    check(copy != pcm && std::memcmp(copy, pcm, sizeof pcm) == 0);
    std::memset(pcm, 0, sizeof pcm);
    check(copy[0] == 1 && nonzero_buffers == 1);

    recomp_audio_output_submit(0, pcm, sizeof pcm, 8000, 1, 16, -2000);
    check(std::fabs(source->volume - 0.1f) < 0.00001f);
    recomp_audio_output_submit(0, pcm, sizeof pcm, 8000, 1, 16, -20000);
    check(source->volume == 0.0f);
    recomp_audio_output_submit(0, pcm, sizeof pcm, 8000, 1, 16, 0);
    check(source->volume == 1.0f);
    recomp_audio_output_submit(0, pcm, sizeof pcm, 8000, 1, 16, -10000);
    check(submitted_buffers == 4 && dropped_buffers == 1 &&
        source->queued.size() == 4 && source->volume == 0.0f);

    const uint8_t *active = source->queued[2].pAudioData;
    source->queued.erase(source->queued.begin(), source->queued.begin() + 2);
    uint8_t larger[16] = {};
    recomp_audio_output_submit(0, larger, sizeof larger, 8000, 1, 16, 0);
    check(source->queued.size() == 3 && source->queued[0].pAudioData == active &&
        voices[0].front == 2 && voices[0].queued == 3);
    recomp_audio_output_reset_voice(0);
    check(source->destroyed && !voices[0].source && allocations.empty());

    std::memset(pcm, 0x80, sizeof pcm);
    recomp_audio_output_submit(1, pcm, sizeof pcm, 8000, 1, 8, 0);
    check(nonzero_buffers == 1);
    pcm[0] = 0;
    recomp_audio_output_submit(1, pcm, sizeof pcm, 8000, 1, 8, 0);
    check(nonzero_buffers == 2 && reported_nonzero[1]);
    source = voices[1].source;
    recomp_audio_output_submit(1, pcm, sizeof pcm, 16000, 2, 16, 0);
    check(source->destroyed && voices[1].sample_rate == 16000 &&
        voices[1].channels == 2 && voices[1].bits_per_sample == 16);

    auto submitted = submitted_buffers;
    recomp_audio_output_submit(256, pcm, sizeof pcm, 8000, 1, 16, 0);
    recomp_audio_output_submit(0, nullptr, sizeof pcm, 8000, 1, 16, 0);
    recomp_audio_output_submit(0, pcm, 160001, 8000, 1, 16, 0);
    recomp_audio_output_submit(0, pcm, sizeof pcm, 999, 1, 16, 0);
    recomp_audio_output_submit(0, pcm, sizeof pcm, 200001, 1, 16, 0);
    recomp_audio_output_submit(0, pcm, sizeof pcm, 8000, 3, 16, 0);
    recomp_audio_output_submit(0, pcm, sizeof pcm, 8000, 1, 24, 0);
    recomp_audio_output_submit(0, pcm, 7, 8000, 2, 16, 0);
    check(submitted_buffers == submitted && dropped_buffers == 9);

    fake_engine.callback->OnCriticalError(XAUDIO2_E_DEVICE_INVALIDATED);
    check(engine && !allocations.empty());
    recomp_audio_output_submit(1, pcm, sizeof pcm, 8000, 1, 8, 0);
    check(!engine && allocations.empty() && submitted_buffers == submitted &&
        release_calls == 2 && com_balance == 0);
    recomp_audio_output_initialize();
    check(create_calls == 3);
    recomp_audio_output_shutdown();
    recomp_audio_output_shutdown();
    check(release_calls == 2);
    for (auto *voice : fake_sources) delete voice;
    std::puts("PASS audio output ownership, FIFO, overflow, volume, teardown, failures");
    return 0;
}
