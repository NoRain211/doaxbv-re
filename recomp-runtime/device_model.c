#include "device_model.h"
#include "kernel_abi.h"
#include "runtime.h"
#include <string.h>

uint32_t recomp_device_disk_query(uint32_t code, void *output,
    uint32_t length, uint32_t *written)
{
    /* Same virtual 16-GiB volume as NtQueryVolumeInformationFile, not a host disk.
       Layouts: nxdk xboxkrnl.h DISK_GEOMETRY and PARTITION_INFORMATION. */
    const uint32_t geometry[6] = {16384u, 0u, 12u, 32u, 64u, 512u};
    const uint32_t partition[8] = {0u, 0u, 0u, 4u, 0u, 1u, 0x00010000u, 0u};
    const void *data;
    uint32_t size;
    *written = 0u;
    if (code == 0x00070000u) {
        data = geometry;
        size = sizeof geometry;
    } else if (code == 0x00074004u) {
        data = partition;
        size = sizeof partition;
    } else {
        return 0xc0000010u;
    }
    if (output == NULL || length < size) return 0xc0000023u;
    memcpy(output, data, size);
    *written = size;
    return 0u;
}

enum {
    DEVICE_OBJECT_SIZE = 0x40u,
    DEVICE_OBJECT_EXTENSION_OFFSET = 0x18u,
    STATUS_SUCCESS = 0x00000000u,
    STATUS_NO_MEMORY = 0xc0000017u,
};

RecompDeviceCreateResult recomp_device_create(
    uint32_t device_extension_size,
    uint32_t device_object_pointer)
{
    uint32_t object = recomp_kernel_allocate_pool(DEVICE_OBJECT_SIZE);
    uint32_t extension = device_extension_size == 0u
        ? 0u
        : recomp_kernel_allocate_pool(device_extension_size);

    if (object != 0u) {
        *recomp_memory_u32(object + DEVICE_OBJECT_EXTENSION_OFFSET) = extension;
    }
    if (device_object_pointer != 0u) {
        *recomp_memory_u32(device_object_pointer) = object;
    }

    if (object == 0u ||
        (device_extension_size != 0u && extension == 0u)) {
        if (object != 0u) {
            recomp_kernel_free_pool(object);
        }
        if (extension != 0u) {
            recomp_kernel_free_pool(extension);
        }
        if (device_object_pointer != 0u) {
            *recomp_memory_u32(device_object_pointer) = 0u;
        }
        return (RecompDeviceCreateResult){
            .status = STATUS_NO_MEMORY,
        };
    }

    return (RecompDeviceCreateResult){
        .status = STATUS_SUCCESS,
        .device_object = object,
        .device_extension = extension,
    };
}
