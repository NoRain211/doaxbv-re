#ifndef DOAXBV_RECOMP_DSOUND_SERVICE_MODEL_H
#define DOAXBV_RECOMP_DSOUND_SERVICE_MODEL_H

#include <stdint.h>

enum {
    RECOMP_DSOUND_MANAGER_SIZE = 0x28u,
    RECOMP_DSOUND_DEVICE_SIZE = 0xa8u,
    RECOMP_DSOUND_APU_SIZE = 0x7e0u,
    RECOMP_DSOUND_VOICE_STATE_SIZE = 0x80u,
    RECOMP_DSOUND_VOICE_STATE_COUNT = 0x100u,
    RECOMP_DSOUND_VOICE_STATE_TABLE_SIZE =
        RECOMP_DSOUND_VOICE_STATE_SIZE * RECOMP_DSOUND_VOICE_STATE_COUNT,
    RECOMP_DSOUND_VOICE_STATE_INDEX_OFFSET = 0x7cu,
    RECOMP_DSOUND_MANAGER_DEVICE_OFFSET = 0x08u,
    RECOMP_DSOUND_MANAGER_APU_OFFSET = 0x0cu,
    RECOMP_DSOUND_MANAGER_LIST_FORWARD_OFFSET = 0x10u,
    RECOMP_DSOUND_MANAGER_LIST_BACK_OFFSET = 0x14u,
    RECOMP_DSOUND_DEVICE_EFFECTS_HANDLE_OFFSET = 0x0cu,
    RECOMP_DSOUND_DEVICE_EFFECTS_HANDLE_NONE = 0xffffffffu,
    RECOMP_DSOUND_APU_INNER_OFFSET = 0x08u,
    RECOMP_DSOUND_APU_DEVICE_OFFSET = 0x0cu,
    RECOMP_DSOUND_APU_MIXER_DEVICE_OFFSET = 0x5cu,
    RECOMP_DSOUND_APU_DEVICE_TAIL_OFFSET = 0xd4u,
    RECOMP_DSOUND_APU_COUNTER_A_POINTER_OFFSET = 0x2f8u,
    RECOMP_DSOUND_APU_COUNTER_B_POINTER_OFFSET = 0x2fcu,
    RECOMP_DSOUND_APU_PAGE_POOL_OFFSET = 0x300u,
    RECOMP_DSOUND_OK = 0x00000000u,
    RECOMP_DSOUND_POINTER_ERROR = 0x80004003u,
    RECOMP_DSOUND_OUT_OF_MEMORY = 0x8007000eu,
    RECOMP_DSOUND_INVALID_PARAM = 0x80070057u,
    RECOMP_DSOUND_PLAY_LOOPING = 1u,
    RECOMP_DSOUND_PLAY_FROMSTART = 2u,
};

typedef struct RecompDsoundBufferModel {
    uint32_t size_bytes;
    uint32_t sample_rate;
    uint32_t original_sample_rate;
    uint32_t block_align;
    uint32_t cursor_bytes;
    uint32_t loop_start_bytes;
    uint32_t frame_remainder;
    uint32_t play_flags;
    uint32_t playing;
    uint64_t last_ms;
} RecompDsoundBufferModel;

uint32_t recomp_dsound_buffer_configure(
    RecompDsoundBufferModel *model, uint32_t size_bytes,
    uint32_t sample_rate, uint32_t block_align, uint64_t now_ms);
uint32_t recomp_dsound_buffer_cursor(
    RecompDsoundBufferModel *model, uint64_t now_ms);
/* Consume elapsed PCM on a separate output clock; wrap at size_bytes to
   loop_start_bytes when looping. A span can cover multiple loop iterations. */
uint32_t recomp_dsound_buffer_consume(
    RecompDsoundBufferModel *model, uint64_t now_ms, uint32_t *offset_bytes);
uint32_t recomp_dsound_buffer_play(
    RecompDsoundBufferModel *model, uint32_t flags, uint64_t now_ms);
void recomp_dsound_buffer_stop(
    RecompDsoundBufferModel *model, uint64_t now_ms);
uint32_t recomp_dsound_buffer_set_position(
    RecompDsoundBufferModel *model, uint32_t position_bytes, uint64_t now_ms);
uint32_t recomp_dsound_buffer_set_frequency(
    RecompDsoundBufferModel *model, uint32_t sample_rate, uint64_t now_ms);

typedef struct RecompDsoundVector {
    float x;
    float y;
    float z;
} RecompDsoundVector;

typedef struct RecompDsoundServiceModel {
    RecompDsoundVector listener_position;
    RecompDsoundVector listener_velocity;
    uint32_t position_apply;
    uint32_t velocity_apply;
    uint32_t work_count;
    uint32_t commit_count;
    uint32_t manager;
    uint32_t device;
    uint32_t apu;
    uint32_t voice_state_table;
    uint32_t public_device;
    uint32_t manager_reference_count;
    uint32_t device_reference_count;
    uint32_t effects_image_buffer;
    uint32_t effects_image_size;
    uint32_t effects_image_location;
    uint32_t effects_image_download_count;
    uint32_t mix_bin;
    uint32_t mix_bin_headroom;
    uint32_t mix_bin_headroom_count;
} RecompDsoundServiceModel;

typedef struct RecompDsoundCreateResources {
    uint32_t manager;
    uint32_t device;
    uint32_t apu;
    uint32_t voice_state_table;
} RecompDsoundCreateResources;

void recomp_dsound_service_reset(RecompDsoundServiceModel *model);
void recomp_dsound_do_work(RecompDsoundServiceModel *model);
void recomp_dsound_set_listener_position(
    RecompDsoundServiceModel *model,
    RecompDsoundVector position,
    uint32_t apply);
void recomp_dsound_set_listener_velocity(
    RecompDsoundServiceModel *model,
    RecompDsoundVector velocity,
    uint32_t apply);
void recomp_dsound_commit_listener(RecompDsoundServiceModel *model);
uint32_t recomp_dsound_download_effects_image(
    RecompDsoundServiceModel *model,
    uint32_t image_buffer,
    uint32_t image_size,
    uint32_t image_location);
void recomp_dsound_set_mix_bin_headroom(
    RecompDsoundServiceModel *model,
    uint32_t mix_bin,
    uint32_t headroom);
uint32_t recomp_dsound_create(
    RecompDsoundServiceModel *model,
    const RecompDsoundCreateResources *resources,
    uint32_t *public_device);

#endif
