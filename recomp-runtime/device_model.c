#include "device_model.h"
#include "kernel_abi.h"
#include "runtime.h"

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
