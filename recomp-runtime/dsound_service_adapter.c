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
    DIRECT_SOUND_APU_VTABLE = 0x00239e44u,
    DIRECT_SOUND_APU_INNER_VTABLE = 0x00239e40u,
    DIRECT_SOUND_APU_PAGE_POOL_VTABLE = 0x00239e98u,
    /* sub_001FB4C2 publishes the addresses of these two counters at APU+0x2F8
       and APU+0x2FC and seeds them. Nothing in this image reads them directly;
       generated code reaches them through the published pointers, so the seed
       values matter even though their meaning is unresolved. */
    DIRECT_SOUND_APU_COUNTER_A_GLOBAL = 0x0021406cu,
    DIRECT_SOUND_APU_COUNTER_B_GLOBAL = 0x00214070u,
    DIRECT_SOUND_APU_COUNTER_A_SEED = 0xc0u,
    DIRECT_SOUND_APU_COUNTER_B_SEED = 0x40u,
    DIRECT_SOUND_APU_PAGE_POOL_TAG = 0x00214074u,
    DIRECT_SOUND_APU_MIXER_DEVICE_FIELD = 0x34u,
    DIRECT_SOUND_APU_DEVICE_TAIL_FIELD = 0x74u,
    DIRECT_SOUND_APU_PAGE_POOL_BLOCK_LIST = 0x04u,
    DIRECT_SOUND_APU_PAGE_POOL_SECOND_LIST = 0x0cu,
    DIRECT_SOUND_APU_PAGE_POOL_TAG_OFFSET = 0x1cu,
    DIRECT_SOUND_APU_TAIL_LIST_FIRST = 0x728u,
    DIRECT_SOUND_APU_TAIL_LIST_LAST = 0x750u,
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

/* A list the constructors leave empty: both links point at the entry itself,
   which is how the generated walkers recognise the end. */
static void write_empty_list(uint32_t entry)
{
    *recomp_memory_u32(entry) = entry;
    *recomp_memory_u32(entry + 4u) = entry;
}

/* The 0x7E0-byte CMcpxAPU that sub_001FB4C2 builds and sub_001FA0E6 links at
   manager+0xC. Without it a sound buffer's APU pointer is zero, and the page
   walk in CMcpxBuffer_Play starts from 0x300 and runs off into unmapped
   memory. Only the constructor's own writes are reproduced, including the
   page pool sub_001FE73B builds at APU+0x300. The sub-objects that need
   their own allocations (APU+0x58) and the hardware bring-up in
   sub_001FBF1C are deliberately absent. */
static void write_apu_object(const RecompDsoundServiceModel *model)
{
    uint32_t apu = model->apu;
    uint32_t page_pool = apu + RECOMP_DSOUND_APU_PAGE_POOL_OFFSET;
    uint32_t offset;

    recomp_guest_memset(apu, 0, RECOMP_DSOUND_APU_SIZE);
    *recomp_memory_u32(apu) = DIRECT_SOUND_APU_VTABLE;
    *recomp_memory_u32(apu + 4u) = 1u;
    *recomp_memory_u32(apu + RECOMP_DSOUND_APU_INNER_OFFSET) =
        DIRECT_SOUND_APU_INNER_VTABLE;
    *recomp_memory_u32(apu + RECOMP_DSOUND_APU_DEVICE_OFFSET) = model->device;
    *recomp_memory_u32(apu + RECOMP_DSOUND_APU_MIXER_DEVICE_OFFSET) =
        model->device + DIRECT_SOUND_APU_MIXER_DEVICE_FIELD;
    *recomp_memory_u32(apu + RECOMP_DSOUND_APU_DEVICE_TAIL_OFFSET) =
        model->device + DIRECT_SOUND_APU_DEVICE_TAIL_FIELD;
    *recomp_memory_u32(apu + RECOMP_DSOUND_APU_COUNTER_A_POINTER_OFFSET) =
        DIRECT_SOUND_APU_COUNTER_A_GLOBAL;
    *recomp_memory_u32(apu + RECOMP_DSOUND_APU_COUNTER_B_POINTER_OFFSET) =
        DIRECT_SOUND_APU_COUNTER_B_GLOBAL;
    *recomp_memory_u32(DIRECT_SOUND_APU_COUNTER_A_GLOBAL) =
        DIRECT_SOUND_APU_COUNTER_A_SEED;
    *recomp_memory_u32(DIRECT_SOUND_APU_COUNTER_B_GLOBAL) =
        DIRECT_SOUND_APU_COUNTER_B_SEED;

    /* sub_001FE73B. The largest-free-block cache at pool+0x18 stays zero, as
       the constructor leaves it, so sub_001FE808 finds no pages and returns
       null rather than mapping anything. Play then fails with
       DSERR_OUTOFMEMORY instead of faulting. */
    *recomp_memory_u32(page_pool) = DIRECT_SOUND_APU_PAGE_POOL_VTABLE;
    write_empty_list(page_pool + DIRECT_SOUND_APU_PAGE_POOL_BLOCK_LIST);
    write_empty_list(page_pool + DIRECT_SOUND_APU_PAGE_POOL_SECOND_LIST);
    *recomp_memory_u32(page_pool + DIRECT_SOUND_APU_PAGE_POOL_TAG_OFFSET) =
        DIRECT_SOUND_APU_PAGE_POOL_TAG;

    /* Six consecutive empty lists; the original writes three in a loop and
       the remaining three one at a time. */
    for (offset = DIRECT_SOUND_APU_TAIL_LIST_FIRST;
         offset <= DIRECT_SOUND_APU_TAIL_LIST_LAST;
         offset += 8u) {
        write_empty_list(apu + offset);
    }
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
        model->manager + RECOMP_DSOUND_MANAGER_APU_OFFSET) = model->apu;
    *recomp_memory_u32(
        model->manager + RECOMP_DSOUND_MANAGER_LIST_FORWARD_OFFSET) =
        list_head;
    *recomp_memory_u32(
        model->manager + RECOMP_DSOUND_MANAGER_LIST_BACK_OFFSET) = list_head;
    *recomp_memory_u32(model->device) = DIRECT_SOUND_DEVICE_VTABLE;
    *recomp_memory_u32(model->device + 4u) =
        model->device_reference_count;
    /* sub_001FB8A7 tests device+0xC against 0xFFFFFFFF before it uses the
       effects path. The guest writes that sentinel when no effects image is
       loaded (sub_001E47A4); a memset leaves 0, which reads as a live handle
       and sends sub_001FB36B chasing the null object at apu+0x14. */
    *recomp_memory_u32(
        model->device + RECOMP_DSOUND_DEVICE_EFFECTS_HANDLE_OFFSET) =
        RECOMP_DSOUND_DEVICE_EFFECTS_HANDLE_NONE;
    write_apu_object(model);
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
        resources.apu = xbox_HeapAlloc(RECOMP_DSOUND_APU_SIZE, 16u);
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
        PRIx32 " device=0x%08" PRIx32 " apu=0x%08" PRIx32 "\n",
        result,
        public_device,
        dsound_service_model.apu);
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
