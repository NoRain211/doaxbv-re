#include "kernel_abi.h"

#include <stdint.h>

static uint32_t av_saved_data_address;

uint32_t recomp_kernel_av_get_saved_data_address(void)
{
    return av_saved_data_address;
}

void recomp_kernel_av_set_saved_data_address(uint32_t address)
{
    av_saved_data_address = address;
}

uint32_t recomp_kernel_av_send_tv_encoder_option(
    uint32_t register_base,
    uint32_t option,
    uint32_t parameter,
    uint32_t result)
{
    (void)register_base;
    (void)option;
    (void)parameter;

    if (result != 0u) {
        *recomp_memory_u32(result) = 0u;
    }
    return 0u;
}

static void bridge_av_get_saved_data_address(void)
{
    kernel_return(0u, recomp_kernel_av_get_saved_data_address());
}

static void bridge_av_set_saved_data_address(void)
{
    recomp_kernel_av_set_saved_data_address(kernel_arg(1u));
    kernel_return(1u, 0u);
}

static void bridge_av_send_tv_encoder_option(void)
{
    uint32_t register_base = kernel_arg(1u);
    uint32_t option = kernel_arg(2u);
    uint32_t parameter = kernel_arg(3u);
    uint32_t result = kernel_arg(4u);

    kernel_return(
        4u,
        recomp_kernel_av_send_tv_encoder_option(
            register_base, option, parameter, result));
}

RecompFunction recomp_kernel_video(uint32_t ordinal)
{
    switch (ordinal) {
    case 1u: return bridge_av_get_saved_data_address;
    case 2u: return bridge_av_send_tv_encoder_option;
    case 4u: return bridge_av_set_saved_data_address;
    default: return NULL;
    }
}
