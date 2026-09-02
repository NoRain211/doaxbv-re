#include "dsound_service_model.h"

#include <stddef.h>

void recomp_dsound_service_reset(RecompDsoundServiceModel *model)
{
    if (model != NULL) {
        *model = (RecompDsoundServiceModel){0};
    }
}

void recomp_dsound_do_work(RecompDsoundServiceModel *model)
{
    if (model != NULL) {
        ++model->work_count;
    }
}

void recomp_dsound_set_listener_position(
    RecompDsoundServiceModel *model,
    RecompDsoundVector position,
    uint32_t apply)
{
    if (model != NULL) {
        model->listener_position = position;
        model->position_apply = apply;
    }
}

void recomp_dsound_set_listener_velocity(
    RecompDsoundServiceModel *model,
    RecompDsoundVector velocity,
    uint32_t apply)
{
    if (model != NULL) {
        model->listener_velocity = velocity;
        model->velocity_apply = apply;
    }
}

void recomp_dsound_commit_listener(RecompDsoundServiceModel *model)
{
    if (model != NULL) {
        ++model->commit_count;
    }
}

uint32_t recomp_dsound_download_effects_image(
    RecompDsoundServiceModel *model,
    uint32_t image_buffer,
    uint32_t image_size,
    uint32_t image_location)
{
    if (model != NULL) {
        model->effects_image_buffer = image_buffer;
        model->effects_image_size = image_size;
        model->effects_image_location = image_location;
        ++model->effects_image_download_count;
    }
    return RECOMP_DSOUND_OK;
}

void recomp_dsound_set_mix_bin_headroom(
    RecompDsoundServiceModel *model,
    uint32_t mix_bin,
    uint32_t headroom)
{
    if (model != NULL) {
        model->mix_bin = mix_bin;
        model->mix_bin_headroom = headroom;
        ++model->mix_bin_headroom_count;
    }
}

uint32_t recomp_dsound_create(
    RecompDsoundServiceModel *model,
    const RecompDsoundCreateResources *resources,
    uint32_t *public_device)
{
    if (public_device != NULL) {
        *public_device = 0u;
    }
    if (model == NULL || public_device == NULL) {
        return RECOMP_DSOUND_POINTER_ERROR;
    }
    if (resources == NULL || resources->manager == 0u ||
        resources->device == 0u || resources->apu == 0u) {
        return RECOMP_DSOUND_OUT_OF_MEMORY;
    }

    model->manager = resources->manager;
    model->device = resources->device;
    model->apu = resources->apu;
    model->public_device =
        resources->manager + RECOMP_DSOUND_MANAGER_DEVICE_OFFSET;
    model->manager_reference_count = 2u;
    model->device_reference_count = 1u;
    *public_device = model->public_device;
    return RECOMP_DSOUND_OK;
}
