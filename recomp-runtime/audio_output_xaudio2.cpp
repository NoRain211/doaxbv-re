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

namespace {

constexpr uint32_t kVoiceCount = 256;
constexpr uint32_t kQueueSize = 16;
constexpr uint32_t kMaxBufferBytes = 160000;
/* Silence queued ahead of a new or starved voice, so producer jitter (15.6 ms
   clock steps, thread wakeups) is absorbed instead of heard as gaps. */
constexpr uint32_t kPrerollMs = 50;
/* XAudio2 does not consume exactly the nominal rate: a 22050 Hz source runs
   0.23% fast without resampling (whole frames per 10 ms pass) and 0.05% slow
   with it. A wall-clock producer would drain or flood the queue, so each voice
   trims its pitch (inaudibly, at most this much) to hold the cushion. */
constexpr float kRateTrim = 0.01f;

struct PcmBuffer {
    uint8_t *data;
    uint32_t capacity;
};

struct OutputVoice {
    IXAudio2SourceVoice *source;
    PcmBuffer buffers[kQueueSize];
    uint32_t sample_rate, channels, bits_per_sample;
    uint32_t front, queued;
    uint64_t bytes_submitted;
    float level_ms;
};

std::atomic<HRESULT> critical_error{S_OK};

struct EngineCallback : IXAudio2EngineCallback {
    void STDMETHODCALLTYPE OnProcessingPassStart() override {}
    void STDMETHODCALLTYPE OnProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnCriticalError(HRESULT error) override
    {
        critical_error.store(error, std::memory_order_relaxed);
    }
};

EngineCallback callback;
IXAudio2 *engine;
IXAudio2MasteringVoice *master;
OutputVoice voices[kVoiceCount];
bool reported_nonzero[kVoiceCount];
bool attempted, com_initialized, callback_registered, summary_printed;
/* Initialized COM; only this thread may release it. */
DWORD owner_thread;
unsigned long long submitted_buffers, submitted_bytes, nonzero_buffers;
unsigned long long dropped_buffers, underruns;
unsigned engine_glitches;

void destroyVoice(OutputVoice &voice)
{
    /* DestroyVoice waits until XAudio2 no longer reads these buffers. */
    if (voice.source) voice.source->DestroyVoice();
    for (auto &buffer : voice.buffers) std::free(buffer.data);
    voice = {};
}

void releaseOutput()
{
    for (auto &voice : voices) destroyVoice(voice);
    if (master) master->DestroyVoice();
    master = nullptr;
    if (engine) {
        XAUDIO2_PERFORMANCE_DATA performance{};
        engine->GetPerformanceData(&performance);
        engine_glitches = performance.GlitchesSinceEngineStarted;
        if (callback_registered) engine->UnregisterForCallbacks(&callback);
        engine->Release();
    }
    engine = nullptr;
    callback_registered = false;
    if (com_initialized) CoUninitialize();
    com_initialized = false;
}

void disableOutput(const char *operation, HRESULT error)
{
    if (GetCurrentThreadId() != owner_thread) {
        /* CoUninitialize is thread-affine: stop output here and let the owner
           thread's next call release it. */
        HRESULT none = S_OK;
        critical_error.compare_exchange_strong(none, error);
        return;
    }
    std::fprintf(stderr,
        "[audio-output] disabled operation=%s error=0x%08lx\n",
        operation, static_cast<unsigned long>(error));
    releaseOutput();
}

void checkCriticalError()
{
    const HRESULT error = critical_error.load(std::memory_order_relaxed);
    if (engine && FAILED(error)) disableOutput("device", error);
}

void dropBuffer(uint32_t slot, const char *reason)
{
    ++dropped_buffers;
    if (dropped_buffers <= 4 ||
        (dropped_buffers & (dropped_buffers - 1)) == 0) {
        std::fprintf(stderr,
            "[audio-output] dropped=%llu slot=%u reason=%s\n",
            dropped_buffers, slot, reason);
    }
}

bool queueBuffer(OutputVoice &voice, uint32_t slot, const uint8_t *pcm,
    uint32_t bytes, uint8_t silence)
{
    if (voice.queued == kQueueSize) {
        dropBuffer(slot, "queue-full");
        return false;
    }
    PcmBuffer &owned = voice.buffers[(voice.front + voice.queued) % kQueueSize];
    if (owned.capacity < bytes) {
        auto *data = static_cast<uint8_t *>(std::realloc(owned.data, bytes));
        if (!data) {
            dropBuffer(slot, "allocation");
            return false;
        }
        owned.data = data;
        owned.capacity = bytes;
    }
    if (pcm) std::memcpy(owned.data, pcm, bytes);
    else std::memset(owned.data, silence, bytes);
    XAUDIO2_BUFFER buffer{};
    buffer.AudioBytes = bytes;
    buffer.pAudioData = owned.data;
    const HRESULT error = voice.source->SubmitSourceBuffer(&buffer);
    if (FAILED(error)) {
        dropBuffer(slot, "submit");
        disableOutput("SubmitSourceBuffer", error);
        return false;
    }
    ++voice.queued;
    voice.bytes_submitted += bytes;
    return true;
}

} // namespace

extern "C" int recomp_audio_output_enabled(void)
{
    return engine != nullptr;
}

extern "C" void recomp_audio_output_initialize(void)
{
    checkCriticalError();
    if (attempted) return;
    attempted = true;
    owner_thread = GetCurrentThreadId();
    double gain = 0.0;
    const char *setting = std::getenv("RECOMP_AUDIO_GAIN");
    if (setting) {
        char *end;
        gain = std::strtod(setting, &end);
        if (end == setting || *end != '\0' || !std::isfinite(gain) ||
            gain < 0.0 || gain > 1.0) {
            std::fprintf(stderr,
                "[audio-output] muted invalid RECOMP_AUDIO_GAIN (expected 0..1)\n");
            return;
        }
    }
    if (gain == 0.0) {
        std::fprintf(stderr, "[audio-output] muted RECOMP_AUDIO_GAIN=0\n");
        return;
    }
    HRESULT error = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(error)) {
        disableOutput("CoInitializeEx", error);
        return;
    }
    com_initialized = true;
    error = XAudio2Create(&engine, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(error)) {
        disableOutput("XAudio2Create", error);
        return;
    }
    error = engine->RegisterForCallbacks(&callback);
    if (FAILED(error)) {
        disableOutput("RegisterForCallbacks", error);
        return;
    }
    callback_registered = true;
    error = engine->CreateMasteringVoice(&master);
    if (FAILED(error)) {
        disableOutput("CreateMasteringVoice", error);
        return;
    }
    error = master->SetVolume(static_cast<float>(gain));
    if (FAILED(error)) {
        disableOutput("MasterSetVolume", error);
        return;
    }
    std::fprintf(stderr,
        "[audio-output] initialized backend=xaudio2_9 master_gain=%.6f\n", gain);
}

extern "C" void recomp_audio_output_shutdown(void)
{
    if (summary_printed) return;
    checkCriticalError();
    releaseOutput();
    attempted = true;
    summary_printed = true;
    std::fprintf(stderr,
        "[audio-output] summary submitted_buffers=%llu submitted_bytes=%llu "
        "nonzero_buffers=%llu dropped_buffers=%llu underruns=%llu "
        "engine_glitches=%u\n",
        submitted_buffers, submitted_bytes, nonzero_buffers, dropped_buffers,
        underruns, engine_glitches);
}

extern "C" void recomp_audio_output_reset_voice(uint32_t slot)
{
    checkCriticalError();
    if (slot < kVoiceCount) destroyVoice(voices[slot]);
}

extern "C" void recomp_audio_output_submit(
    uint32_t slot, const uint8_t *pcm, uint32_t bytes,
    uint32_t sample_rate, uint32_t channels, uint32_t bits_per_sample,
    int32_t volume_hundredth_db)
{
    recomp_audio_output_initialize();
    if (!engine || bytes == 0 ||
        FAILED(critical_error.load(std::memory_order_relaxed))) return;
    if (slot >= kVoiceCount || !pcm || bytes > kMaxBufferBytes ||
        sample_rate < XAUDIO2_MIN_SAMPLE_RATE ||
        sample_rate > XAUDIO2_MAX_SAMPLE_RATE ||
        (channels != 1 && channels != 2) ||
        (bits_per_sample != 8 && bits_per_sample != 16)) {
        dropBuffer(slot, "invalid-pcm");
        return;
    }
    const uint32_t block_align = channels * (bits_per_sample / 8);
    if (bytes % block_align != 0) {
        dropBuffer(slot, "partial-frame");
        return;
    }

    OutputVoice &voice = voices[slot];
    if (voice.source && (voice.sample_rate != sample_rate ||
        voice.channels != channels || voice.bits_per_sample != bits_per_sample)) {
        destroyVoice(voice);
    }
    if (!voice.source) {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = static_cast<WORD>(channels);
        format.nSamplesPerSec = sample_rate;
        format.nAvgBytesPerSec = sample_rate * block_align;
        format.nBlockAlign = static_cast<WORD>(block_align);
        format.wBitsPerSample = static_cast<WORD>(bits_per_sample);
        HRESULT error = engine->CreateSourceVoice(
            &voice.source, &format, 0, 1.0f + kRateTrim);
        if (FAILED(error)) {
            dropBuffer(slot, "create-voice");
            disableOutput("CreateSourceVoice", error);
            return;
        }
        voice.sample_rate = sample_rate;
        voice.channels = channels;
        voice.bits_per_sample = bits_per_sample;
        voice.level_ms = kPrerollMs;
        error = voice.source->Start();
        if (FAILED(error)) {
            dropBuffer(slot, "start-voice");
            disableOutput("Start", error);
            return;
        }
    }

    if (volume_hundredth_db < -10000) volume_hundredth_db = -10000;
    if (volume_hundredth_db > 0) volume_hundredth_db = 0;
    const float gain = volume_hundredth_db == -10000 ? 0.0f :
        std::pow(10.0f, volume_hundredth_db / 2000.0f);
    HRESULT error = voice.source->SetVolume(gain);
    if (FAILED(error)) {
        dropBuffer(slot, "volume");
        disableOutput("SetVolume", error);
        return;
    }
    XAUDIO2_VOICE_STATE state{};
    voice.source->GetState(&state, 0);
    const bool starved = state.BuffersQueued == 0;
    if (starved && voice.queued > 0) ++underruns;
    /* BuffersQueued includes the active buffer; only retire the FIFO prefix. */
    while (voice.queued > state.BuffersQueued) {
        voice.front = (voice.front + 1) % kQueueSize;
        --voice.queued;
    }
    const uint8_t silence = bits_per_sample == 8 ? 0x80 : 0;
    if (starved && !queueBuffer(voice, slot, nullptr,
            sample_rate * kPrerollMs / 1000 * block_align, silence)) {
        return;
    }
    const float queued_ms = static_cast<float>(
        voice.bytes_submitted / block_align - state.SamplesPlayed) * 1000.0f / sample_rate;
    voice.level_ms += (queued_ms - voice.level_ms) / 16.0f; /* smooths pump jitter */
    float ratio = 1.0f + (voice.level_ms - kPrerollMs) / 10000.0f;
    if (ratio < 1.0f - kRateTrim) ratio = 1.0f - kRateTrim;
    if (ratio > 1.0f + kRateTrim) ratio = 1.0f + kRateTrim;
    error = voice.source->SetFrequencyRatio(ratio);
    if (FAILED(error)) {
        dropBuffer(slot, "rate");
        disableOutput("SetFrequencyRatio", error);
        return;
    }
    if (!queueBuffer(voice, slot, pcm, bytes, silence)) return;
    ++submitted_buffers;
    submitted_bytes += bytes;
    bool nonzero = false;
    for (uint32_t i = 0; i < bytes && !nonzero; ++i)
        nonzero = pcm[i] != silence;
    if (nonzero) ++nonzero_buffers;
    if (submitted_buffers == 1 || (nonzero && !reported_nonzero[slot])) {
        std::fprintf(stderr,
            "[audio-output] submitted slot=%u bytes=%u rate=%u channels=%u "
            "bits=%u nonzero=%u gain=%.6f\n",
            slot, bytes, sample_rate, channels, bits_per_sample,
            nonzero ? 1u : 0u, static_cast<double>(gain));
    }
    if (nonzero) reported_nonzero[slot] = true;
}
