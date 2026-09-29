#include "soundtrack_adapter.h"
#include "custom_music.h"
#include "kernel_abi.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Generated originals the adapter wraps or falls through to. */
void sub_0018145F(void); /* XFindClose */
void sub_00188210(void); /* CRI soundtrack start */
void sub_001880D0(void); /* CRI soundtrack service callback */
void sub_0018D270(void); /* WMASJD context stop */

enum {
    GUEST_SET_LAST_ERROR = 0x00183183u,
    WMASJD_DECODE_STEP = 0x0018cdd0u,
    WMA_XMO_VTABLE = 0x0023f88cu
};
enum {
    GUEST_ERROR_FILE_NOT_FOUND = 2u,
    GUEST_ERROR_INVALID_HANDLE = 6u,
    GUEST_ERROR_NOT_ENOUGH_MEMORY = 8u,
    GUEST_ERROR_NO_MORE_FILES = 18u,
    GUEST_ERROR_INVALID_PARAMETER = 87u
};
static const uint32_t guest_e_invalidarg = 0x80070057u;
static const uint32_t guest_e_fail = 0x80004005u;

/* CRI's WMASJD contexts are a fixed static array of four (0x60 bytes apart),
   so a table keyed by context address never needs more than four slots. */
enum { SOUNDTRACK_ID = 1u, MAX_CONTEXTS = 4 };

/* CRI player -> work -> audio -> WMASJD decoder context. */
enum { PLAYER_WORK = 0x04u, WORK_AUDIO = 0x04u, AUDIO_CONTEXT = 0xbcu };
/* WMASJD decoder context. The original stop takes CONTEXT_LOCK with a
   sleeping spin, sets CONTEXT_STOP_REQUEST, and waits for the worker to clear
   CONTEXT_WORKER_BUSY while it drives the other contexts. */
enum {
    CONTEXT_STATE = 0x04u,
    CONTEXT_LOCK = 0x08u,
    CONTEXT_STOP_REQUEST = 0x10u,
    CONTEXT_WORKER_BUSY = 0x1cu
};
enum {
    STATE_STOPPED = 0u,
    STATE_STARTING = 1u,
    STATE_DECODING = 2u,
    STATE_ENDED = 3u,
    STATE_FAILED = 4u
};

#pragma pack(push, 1)
typedef struct GuestSoundtrackData { /* XSOUNDTRACK_DATA */
    uint32_t id, song_count, length_ms;
    uint16_t name[32];
} GuestSoundtrackData;
typedef struct GuestMediaPacket { /* XMEDIAPACKET */
    uint32_t buffer, size, completed_size, status, completion_event, timestamp;
} GuestMediaPacket;
typedef struct GuestMediaInfo { /* XMEDIAINFO */
    uint32_t flags, input_size, output_size, max_lookahead;
} GuestMediaInfo;
typedef struct GuestWaveFormat { /* WAVEFORMATEX */
    uint16_t format_tag, channels;
    uint32_t samples_per_second, bytes_per_second;
    uint16_t block_align, bits_per_sample, extra_size;
} GuestWaveFormat;
#pragma pack(pop)
_Static_assert(sizeof(GuestSoundtrackData) == 76, "XSOUNDTRACK_DATA");
_Static_assert(sizeof(GuestMediaPacket) == 24, "XMEDIAPACKET");
_Static_assert(sizeof(GuestMediaInfo) == 16, "XMEDIAINFO");
_Static_assert(sizeof(GuestWaveFormat) == 18, "WAVEFORMATEX");

/* Song chosen for each context at start; overwritten by every restart. */
static struct { uint32_t context, song; } bindings[MAX_CONTEXTS];
/* Live XMO objects, allocated on create and freed on final release. */
static struct {
    uint32_t object, references, song;
    RecompMusicDecoder *decoder;
} xmos[MAX_CONTEXTS];
/* The one soundtrack has one enumerator token, shared by open enumerations. */
static uint32_t enumerator, open_enumerations;

void recomp_soundtrack_shutdown(void)
{
    for (size_t i = 0; i < MAX_CONTEXTS; ++i) recomp_music_close(xmos[i].decoder);
    memset(bindings, 0, sizeof bindings);
    memset(xmos, 0, sizeof xmos);
    enumerator = open_enumerations = 0;
}

static uint32_t guest_u32(uint32_t address) { return *recomp_memory_u32(address); }

static void last_error(uint32_t error)
{
    kernel_call_guest(GUEST_SET_LAST_ERROR, &error, 1u);
}

static int own_enumerator(uint32_t handle)
{
    return enumerator && handle == enumerator && open_enumerations;
}

static void find_first(void)
{
    const uint32_t output = kernel_arg(1u);
    uint32_t error = 0;
    if (!output) error = GUEST_ERROR_INVALID_PARAMETER;
    else if (!recomp_music_count()) error = GUEST_ERROR_NO_MORE_FILES;
    else if (!enumerator && !(enumerator = recomp_kernel_allocate_pool(4u)))
        error = GUEST_ERROR_NOT_ENOUGH_MEMORY;
    if (error) {
        last_error(error);
        kernel_return(1u, UINT32_MAX);
        return;
    }
    GuestSoundtrackData data = {SOUNDTRACK_ID, recomp_music_count(), recomp_music_duration(),
        {'U', 's', 'e', 'r', 'M', 'u', 's', 'i', 'c'}};
    memcpy(recomp_memory(output, sizeof data), &data, sizeof data);
    ++open_enumerations;
    kernel_return(1u, enumerator);
}

static void find_next(void)
{
    last_error(own_enumerator(kernel_arg(1u)) ? GUEST_ERROR_NO_MORE_FILES
                                              : GUEST_ERROR_INVALID_HANDLE);
    kernel_return(2u, 0u);
}

static void find_close(void)
{
    if (!own_enumerator(kernel_arg(1u))) {
        sub_0018145F();
        return;
    }
    --open_enumerations;
    kernel_return(1u, 1u);
}

static void song_info(void)
{
    const RecompMusicTrack *track = recomp_music_track(kernel_arg(2u));
    const uint32_t id = kernel_arg(3u), duration = kernel_arg(4u);
    const uint32_t name = kernel_arg(5u), capacity = kernel_arg(6u);
    if (kernel_arg(1u) != SOUNDTRACK_ID || !track || !id || !duration ||
            (name && !capacity)) {
        last_error(GUEST_ERROR_INVALID_PARAMETER);
        kernel_return(6u, 0u);
        return;
    }
    *recomp_memory_u32(id) = track->id;
    *recomp_memory_u32(duration) = track->duration_ms;
    if (name) {
        uint32_t count = capacity < RECOMP_MUSIC_NAME_UNITS ? capacity : RECOMP_MUSIC_NAME_UNITS;
        uint16_t *out = (uint16_t *)recomp_memory(name, count * 2u);
        memcpy(out, track->name, count * 2u);
        out[count - 1u] = 0;
    }
    kernel_return(6u, 1u);
}

/* CRI sends any file that starts with "RIFF" to its own WAV reader. Hiding the
   tag keeps every song on the XMO path; the host file is never written. */
static void hide_riff(uint64_t offset, uint8_t *bytes, uint32_t count)
{
    for (uint64_t at = offset; at < 4u && at - offset < count; ++at) bytes[at - offset] = 0;
}

static void open_song(void)
{
    const wchar_t *path = recomp_music_path(kernel_arg(1u));
    uint32_t handle = path ? recomp_kernel_open_readonly(path, hide_riff) : 0u;
    if (!handle) {
        last_error(GUEST_ERROR_FILE_NOT_FOUND);
        handle = UINT32_MAX;
    }
    kernel_return(2u, handle);
}

static uint32_t player_context(uint32_t player)
{
    const uint32_t work = player ? guest_u32(player + PLAYER_WORK) : 0u;
    const uint32_t audio = work ? guest_u32(work + WORK_AUDIO) : 0u;
    return audio ? guest_u32(audio + AUDIO_CONTEXT) : 0u;
}

static int binding_index(uint32_t context)
{
    if (context) for (int i = 0; i < MAX_CONTEXTS; ++i)
        if (bindings[i].context == context) return i;
    return -1;
}

/* Binds the selected song to its context before the worker creates the XMO. */
static void start_song(void)
{
    const uint32_t context = player_context(kernel_arg(1u));
    int i = binding_index(context);
    for (int slot = 0; context && slot < MAX_CONTEXTS && i < 0; ++slot)
        if (!bindings[slot].context) i = slot;
    if (i >= 0) {
        bindings[i].context = context;
        bindings[i].song = kernel_arg(2u);
    }
    sub_00188210();
}

/* Guest worker threads never run on the host, so CRI's service callback also
   drives one synchronous WMASJD decode step for each bound context. The step
   (and the XMO calls it makes) never waits on another thread; it only reads
   the encoded ring and calls ProcessMultiple. */
static void service_decoder(void)
{
    const uint32_t audio = kernel_arg(1u);
    const uint32_t context = audio ? guest_u32(audio + AUDIO_CONTEXT) : 0u;
    const int bound = binding_index(context) >= 0;
    if (bound && guest_u32(context + CONTEXT_STATE) == STATE_FAILED) {
        /* The game advances on completion; a failed file must not leave its
           music channel waiting forever for another packet. */
        fprintf(stderr, "[custom-music] ending failed track\n");
        *recomp_memory_u32(context + CONTEXT_STATE) = STATE_ENDED;
    }
    sub_001880D0();
    if (!bound) return;
    const uint32_t state = guest_u32(context + CONTEXT_STATE);
    if ((state != STATE_STARTING && state != STATE_DECODING) ||
            guest_u32(context + CONTEXT_STOP_REQUEST)) return;
    RecompRegisters saved = recomp_runtime.registers;
    RecompFpuContext fpu;
    recomp_fpu_context_save(&fpu);
    kernel_call_guest(WMASJD_DECODE_STEP, &context, 1u);
    recomp_fpu_context_restore(&fpu);
    recomp_runtime.registers = saved;
}

static void stop_decoder(void)
{
    const uint32_t context = kernel_arg(1u);
    if (binding_index(context) < 0) {
        sub_0018D270();
        return;
    }
    /* Decode steps are synchronous, so no worker holds the lock or is busy.
       CRI's own stop still halts its DirectSound buffers and file reads, and
       its restart/detach path releases the XMO. */
    *recomp_memory_u32(context + CONTEXT_STATE) = STATE_STOPPED;
    *recomp_memory_u32(context + CONTEXT_LOCK) = 0u;
    *recomp_memory_u32(context + CONTEXT_STOP_REQUEST) = 1u;
    *recomp_memory_u32(context + CONTEXT_WORKER_BUSY) = 0u;
    kernel_return_caller_cleanup(0u);
}

static int xmo_index(uint32_t object)
{
    if (object) for (int i = 0; i < MAX_CONTEXTS; ++i)
        if (xmos[i].object == object) return i;
    return -1;
}

static void create_decoder(void)
{
    const uint32_t context = kernel_arg(2u), format = kernel_arg(4u), output = kernel_arg(5u);
    if (output) *recomp_memory_u32(output) = 0u;
    if (!kernel_arg(1u) || !format || !output) {
        kernel_return(5u, guest_e_invalidarg);
        return;
    }
    const int binding = binding_index(context);
    int i = -1;
    for (int slot = 0; slot < MAX_CONTEXTS && i < 0; ++slot)
        if (!xmos[slot].object) i = slot;
    if (binding < 0 || i < 0) {
        fprintf(stderr, "[custom-music] no decoder for context=%08x\n", context);
        kernel_return(5u, guest_e_fail);
        return;
    }
    const uint32_t song = bindings[binding].song;
    RecompMusicDecoder *decoder = recomp_music_open(song);
    const uint32_t object = decoder ? recomp_kernel_allocate_pool(4u) : 0u;
    if (!object) {
        recomp_music_close(decoder);
        fprintf(stderr, "[custom-music] decoder song=%08x open failed\n", song);
        kernel_return(5u, guest_e_fail);
        return;
    }
    xmos[i].object = object;
    xmos[i].references = 1u;
    xmos[i].song = song;
    xmos[i].decoder = decoder;
    *recomp_memory_u32(object) = WMA_XMO_VTABLE;
    const GuestWaveFormat wave = {1u, 2u, RECOMP_MUSIC_RATE,
        RECOMP_MUSIC_RATE * RECOMP_MUSIC_FRAME_BYTES, RECOMP_MUSIC_FRAME_BYTES, 16u, 0u};
    memcpy(recomp_memory(format, sizeof wave), &wave, sizeof wave);
    *recomp_memory_u32(output) = object;
    fprintf(stderr, "[custom-music] decoder song=%08x opened\n", song);
    kernel_return(5u, 0u);
}

static void decoder_addref(void)
{
    const int i = xmo_index(kernel_arg(1u));
    kernel_return(1u, i < 0 ? 0u : ++xmos[i].references);
}

static void decoder_release(void)
{
    const int i = xmo_index(kernel_arg(1u));
    uint32_t count = 0;
    if (i >= 0 && !(count = --xmos[i].references)) {
        recomp_music_close(xmos[i].decoder);
        recomp_kernel_free_pool(xmos[i].object);
        memset(&xmos[i], 0, sizeof xmos[i]);
    }
    kernel_return(1u, count);
}

static void decoder_process(void)
{
    const int i = xmo_index(kernel_arg(1u));
    const uint32_t address = kernel_arg(3u);
    uint32_t result = guest_e_invalidarg;
    if (address) {
        GuestMediaPacket packet;
        memcpy(&packet, recomp_memory(address, sizeof packet), sizeof packet);
        if (packet.completed_size) *recomp_memory_u32(packet.completed_size) = 0;
        const int valid = i >= 0 && kernel_arg(2u) == 0 && packet.buffer && packet.size &&
            packet.size <= RECOMP_MUSIC_MAX_READ && packet.size % RECOMP_MUSIC_FRAME_BYTES == 0;
        if (!valid)
            fprintf(stderr, "[custom-music] rejected packet object=%08x size=%u\n",
                kernel_arg(1u), packet.size);
        else {
            const int bytes = recomp_music_read(xmos[i].decoder,
                recomp_memory(packet.buffer, packet.size), packet.size);
            result = bytes < 0 ? guest_e_fail : 0u;
            if (bytes < 0)
                fprintf(stderr, "[custom-music] decoder song=%08x decode failed\n", xmos[i].song);
            else if (packet.completed_size)
                *recomp_memory_u32(packet.completed_size) = (uint32_t)bytes;
        }
        if (packet.status) *recomp_memory_u32(packet.status) = result;
    }
    kernel_return(3u, result);
}

static void decoder_info(void)
{
    const uint32_t output = kernel_arg(2u);
    uint32_t result = guest_e_invalidarg;
    if (xmo_index(kernel_arg(1u)) >= 0 && output) {
        /* Fixed sample size and packet alignment: whole PCM frames. */
        const GuestMediaInfo info = {3u, 0u, RECOMP_MUSIC_FRAME_BYTES, 0u};
        memcpy(recomp_memory(output, sizeof info), &info, sizeof info);
        result = 0u;
    }
    kernel_return(2u, result);
}

static void decoder_status(void)
{
    const uint32_t output = kernel_arg(2u);
    uint32_t result = guest_e_invalidarg;
    if (xmo_index(kernel_arg(1u)) >= 0 && output) {
        *recomp_memory_u32(output) = 2u; /* accepts output packets */
        result = 0u;
    }
    kernel_return(2u, result);
}

static void decoder_discontinuity(void)
{
    kernel_return(1u, xmo_index(kernel_arg(1u)) >= 0 ? 0u : guest_e_invalidarg);
}

static void decoder_flush(void)
{
    const int i = xmo_index(kernel_arg(1u));
    kernel_return(1u, i >= 0 && recomp_music_rewind(xmos[i].decoder) ? 0u : guest_e_fail);
}

RecompFunction recomp_soundtrack_lookup_manual(uint32_t address)
{
    switch (address) {
    case 0x0018145fu: return find_close;
    case 0x00182411u: return find_next;
    case 0x00182430u: return find_first;
    case 0x001824dfu: return song_info;
    case 0x00182708u: return open_song;
    case 0x001880d0u: return service_decoder;
    case 0x00188210u: return start_song;
    case 0x0018d270u: return stop_decoder;
    case 0x0021972au: return create_decoder;
    case 0x0021945du: return decoder_addref;
    case 0x0021965au: return decoder_release;
    case 0x0021924cu: return decoder_info;
    case 0x00219271u: return decoder_status;
    case 0x002194b0u: return decoder_process;
    case 0x0021926cu: return decoder_discontinuity;
    case 0x00218f16u: return decoder_flush;
    default: return NULL;
    }
}
