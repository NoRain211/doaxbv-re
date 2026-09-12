#ifndef DOAXBV_RECOMP_DEVICE_MODEL_H
#define DOAXBV_RECOMP_DEVICE_MODEL_H

#include <stdint.h>

typedef struct RecompDeviceCreateResult {
    uint32_t status;
    uint32_t device_object;
    uint32_t device_extension;
} RecompDeviceCreateResult;

RecompDeviceCreateResult recomp_device_create(
    uint32_t device_extension_size,
    uint32_t device_object_pointer);

uint32_t recomp_device_disk_query(uint32_t code, void *output,
    uint32_t length, uint32_t *written);

#endif
