#include "device_model.h"
#include "kernel_abi.h"

#include <stdint.h>
#include <stdio.h>

static void bridge_io_create_device(void)
{
    uint32_t driver_object = kernel_arg(1u);
    uint32_t extension_size = kernel_arg(2u);
    uint32_t device_name = kernel_arg(3u);
    uint32_t characteristics = kernel_arg(4u);
    uint32_t exclusive = kernel_arg(5u);
    uint32_t device_object_pointer = kernel_arg(6u);
    RecompDeviceCreateResult result = recomp_device_create(
        extension_size, device_object_pointer);

    fprintf(
        stderr,
        "recomp kernel: IoCreateDevice driver=0x%08x"
        " extension_size=%u name=0x%08x characteristics=0x%08x"
        " exclusive=%u output=0x%08x object=0x%08x extension=0x%08x"
        " status=0x%08x\n",
        (unsigned)driver_object,
        (unsigned)extension_size,
        (unsigned)device_name,
        (unsigned)characteristics,
        (unsigned)exclusive,
        (unsigned)device_object_pointer,
        (unsigned)result.device_object,
        (unsigned)result.device_extension,
        (unsigned)result.status);
    kernel_return(6u, result.status);
}

RecompFunction recomp_kernel_device(uint32_t ordinal)
{
    switch (ordinal) {
    case 65u: return bridge_io_create_device;
    default: return NULL;
    }
}
