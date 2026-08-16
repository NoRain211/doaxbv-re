#include "dsound_service_adapter.h"
#include "xbox_memory_layout.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum {
    DIRECT_SOUND_CREATE_ADDRESS = 0x001fa27cu,
    DIRECT_SOUND_COMMIT_DEFERRED_SETTINGS_ADDRESS = 0x001f974fu,
    DIRECT_SOUND_DO_WORK_ADDRESS = 0x001f90e0u,
    DIRECT_SOUND_DOWNLOAD_EFFECTS_IMAGE_ADDRESS = 0x001f8f21u,
    DIRECT_SOUND_SET_MIX_BIN_HEADROOM_ADDRESS = 0x001f8f48u,
    DIRECT_SOUND_SET_POSITION_ADDRESS = 0x001f9dd4u,
    DIRECT_SOUND_SET_VELOCITY_ADDRESS = 0x001f9e09u,
    DIRECT_SOUND_MANAGER_GLOBAL = 0x00214708u,
    DIRECT_SOUND_MANAGER_VTABLE = 0x00239decu,
    DIRECT_SOUND_DEVICE_VTABLE = 0x00239e1cu,
};

static RecompDsoundServiceModel dsound_service_model;

static uint32_t stack_argument(uint32_t entry_esp, uint32_t index)
{
    return *recomp_memory_u32(entry_esp + 4u + index * 4u);
}

static float stack_float(uint32_t entry_esp, uint32_t index)
{
    uint32_t bits = stack_argument(entry_esp, index);
    float value;

    memcpy(&value, &bits, sizeof value);
    return value;
}

static void write_created_objects(const RecompDsoundServiceModel *model)
{
    uint32_t list_head =
        model->manager + RECOMP_DSOUND_MANAGER_LIST_FORWARD_OFFSET;

    /* Current generated consumers require the sub_001F855B manager header and
       the sub_001F987D device header. sub_001FA106 links the device at
       manager+8; the returned public interface is that cell. */
    recomp_guest_memset(model->manager, 0, RECOMP_DSOUND_MANAGER_SIZE);
    recomp_guest_memset(model->device, 0, RECOMP_DSOUND_DEVICE_SIZE);
    *recomp_memory_u32(model->manager) = DIRECT_SOUND_MANAGER_VTABLE;
    *recomp_memory_u32(model->manager + 4u) =
        model->manager_reference_count;
    *recomp_memory_u32(
        model->manager + RECOMP_DSOUND_MANAGER_DEVICE_OFFSET) = model->device;
    *recomp_memory_u32(
        model->manager + RECOMP_DSOUND_MANAGER_LIST_FORWARD_OFFSET) =
        list_head;
    *recomp_memory_u32(
        model->manager + RECOMP_DSOUND_MANAGER_LIST_BACK_OFFSET) = list_head;
    *recomp_memory_u32(model->device) = DIRECT_SOUND_DEVICE_VTABLE;
    *recomp_memory_u32(model->device + 4u) =
        model->device_reference_count;
    *recomp_memory_u32(DIRECT_SOUND_MANAGER_GLOBAL) = model->manager;
}

static void recomp_dsound_create_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t output_address = stack_argument(entry_esp, 1u);
    uint32_t heap_checkpoint = xbox_HeapCheckpoint();
    RecompDsoundCreateResources resources = {0};
    uint32_t public_device = 0u;
    uint32_t result = RECOMP_DSOUND_POINTER_ERROR;

    if (output_address != 0u) {
        *recomp_memory_u32(output_address) = 0u;
        resources.manager = xbox_HeapAlloc(RECOMP_DSOUND_MANAGER_SIZE, 16u);
        resources.device = xbox_HeapAlloc(RECOMP_DSOUND_DEVICE_SIZE, 16u);
        result = recomp_dsound_create(
            &dsound_service_model, &resources, &public_device);
    }
    if (result == RECOMP_DSOUND_OK) {
        write_created_objects(&dsound_service_model);
        *recomp_memory_u32(output_address) = public_device;
    } else if (resources.manager != 0u &&
               !xbox_HeapRestore(heap_checkpoint)) {
        fprintf(stderr, "recomp dsound: could not restore guest heap\n");
    }

    fprintf(
        stderr,
        "recomp dsound: DirectSoundCreate policy=no-audio result=0x%08"
        PRIx32 " device=0x%08" PRIx32 "\n",
        result,
        public_device);
    recomp_runtime.registers.eax = result;
    recomp_runtime.registers.esp = entry_esp + 16u;
}

void recomp_dsound_service_adapter_reset(void)
{
    recomp_dsound_service_reset(&dsound_service_model);
}

const RecompDsoundServiceModel *recomp_dsound_service_adapter_model(void)
{
    return &dsound_service_model;
}

static void recomp_dsound_do_work_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;

    recomp_dsound_do_work(&dsound_service_model);
    if (dsound_service_model.work_count == 1u) {
        fprintf(
            stderr,
            "recomp dsound: DirectSoundDoWork policy=no-audio-service"
            " count=%" PRIu32 "\n",
            dsound_service_model.work_count);
    }
    recomp_runtime.registers.esp = entry_esp + 4u;
}

static void recomp_dsound_download_effects_image_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t image_description = stack_argument(entry_esp, 4u);

    recomp_runtime.registers.eax = recomp_dsound_download_effects_image(
        &dsound_service_model,
        stack_argument(entry_esp, 1u),
        stack_argument(entry_esp, 2u),
        stack_argument(entry_esp, 3u));
    if (image_description != 0u) {
        *recomp_memory_u32(image_description) = 0u;
    }
    fprintf(
        stderr,
        "recomp dsound: DownloadEffectsImage policy=no-audio"
        " buffer=0x%08" PRIx32 " size=%" PRIu32 " result=0x%08"
        PRIx32 "\n",
        dsound_service_model.effects_image_buffer,
        dsound_service_model.effects_image_size,
        recomp_runtime.registers.eax);
    recomp_runtime.registers.esp = entry_esp + 24u;
}

static void recomp_dsound_set_mix_bin_headroom_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;

    recomp_dsound_set_mix_bin_headroom(
        &dsound_service_model,
        stack_argument(entry_esp, 1u),
        stack_argument(entry_esp, 2u));
    recomp_runtime.registers.eax = RECOMP_DSOUND_OK;
    recomp_runtime.registers.esp = entry_esp + 16u;
}

static void recomp_dsound_set_position_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    RecompDsoundVector position = {
        .x = stack_float(entry_esp, 1u),
        .y = stack_float(entry_esp, 2u),
        .z = stack_float(entry_esp, 3u),
    };

    recomp_dsound_set_listener_position(
        &dsound_service_model, position, stack_argument(entry_esp, 4u));
    recomp_runtime.registers.eax = RECOMP_DSOUND_OK;
    recomp_runtime.registers.esp = entry_esp + 24u;
}

static void recomp_dsound_set_velocity_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    RecompDsoundVector velocity = {
        .x = stack_float(entry_esp, 1u),
        .y = stack_float(entry_esp, 2u),
        .z = stack_float(entry_esp, 3u),
    };

    recomp_dsound_set_listener_velocity(
        &dsound_service_model, velocity, stack_argument(entry_esp, 4u));
    recomp_runtime.registers.eax = RECOMP_DSOUND_OK;
    recomp_runtime.registers.esp = entry_esp + 24u;
}

static void recomp_dsound_commit_deferred_settings_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;

    recomp_dsound_commit_listener(&dsound_service_model);
    recomp_runtime.registers.eax = RECOMP_DSOUND_OK;
    recomp_runtime.registers.esp = entry_esp + 8u;
}

RecompFunction recomp_dsound_service_lookup_manual(uint32_t guest_address)
{
    switch (guest_address) {
    case DIRECT_SOUND_CREATE_ADDRESS:
        return recomp_dsound_create_adapter;
    case DIRECT_SOUND_DO_WORK_ADDRESS:
        return recomp_dsound_do_work_adapter;
    case DIRECT_SOUND_DOWNLOAD_EFFECTS_IMAGE_ADDRESS:
        return recomp_dsound_download_effects_image_adapter;
    case DIRECT_SOUND_SET_MIX_BIN_HEADROOM_ADDRESS:
        return recomp_dsound_set_mix_bin_headroom_adapter;
    case DIRECT_SOUND_COMMIT_DEFERRED_SETTINGS_ADDRESS:
        return recomp_dsound_commit_deferred_settings_adapter;
    case DIRECT_SOUND_SET_POSITION_ADDRESS:
        return recomp_dsound_set_position_adapter;
    case DIRECT_SOUND_SET_VELOCITY_ADDRESS:
        return recomp_dsound_set_velocity_adapter;
    default:
        return NULL;
    }
}
