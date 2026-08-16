#ifndef DOAXBV_RECOMP_DSOUND_SERVICE_MODEL_H
#define DOAXBV_RECOMP_DSOUND_SERVICE_MODEL_H

#include <stdint.h>

enum {
    RECOMP_DSOUND_MANAGER_SIZE = 0x28u,
    RECOMP_DSOUND_DEVICE_SIZE = 0xa8u,
    RECOMP_DSOUND_MANAGER_DEVICE_OFFSET = 0x08u,
    RECOMP_DSOUND_MANAGER_LIST_FORWARD_OFFSET = 0x10u,
    RECOMP_DSOUND_MANAGER_LIST_BACK_OFFSET = 0x14u,
    RECOMP_DSOUND_OK = 0x00000000u,
    RECOMP_DSOUND_POINTER_ERROR = 0x80004003u,
    RECOMP_DSOUND_OUT_OF_MEMORY = 0x8007000eu,
};

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
