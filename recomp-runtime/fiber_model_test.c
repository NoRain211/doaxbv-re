#include "fiber_model.h"

#include <stdio.h>

static int expect_u32(const char *field, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "fiber model: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

int recomp_fiber_model_test(void)
{
    const uint32_t main_handle = 0x00740508u;
    const uint32_t worker_handle = 0x01003ff0u;
    const RecompRegisters main_registers = {
        .eax = 1u,
        .ebx = 2u,
        .esi = 3u,
        .edi = 4u,
        .ebp = 5u,
        .esp = 0x00f7ff00u,
    };
    const RecompRegisters worker_registers = {
        .eax = 6u,
        .ebx = 7u,
        .esi = 8u,
        .edi = 9u,
        .ebp = 10u,
        .esp = 0x01003fb0u,
    };
    RecompFiberModel model;
    RecompFiber *main_fiber;
    RecompFiber *worker_fiber;
    int passed = 1;

    recomp_fiber_model_reset(&model);
    main_fiber = recomp_fiber_add(
        &model,
        main_handle,
        0u,
        0x11111111u,
        &main_registers,
        0xffffffffu);
    worker_fiber = recomp_fiber_add(
        &model,
        worker_handle,
        0x000b5570u,
        0x005f2c38u,
        &worker_registers,
        0x22222222u);

    passed &= expect_u32("main added", main_fiber != NULL, 1u);
    passed &= expect_u32("worker added", worker_fiber != NULL, 1u);
    passed &= expect_u32(
        "duplicate rejected",
        recomp_fiber_add(
            &model,
            worker_handle,
            0u,
            0u,
            &worker_registers,
            0u) != NULL,
        0u);
    passed &= expect_u32(
        "select main",
        recomp_fiber_set_current(&model, main_handle),
        1u);
    passed &= expect_u32(
        "current main",
        recomp_fiber_current(&model)->guest_handle,
        main_handle);
    passed &= expect_u32(
        "main ebx isolated",
        recomp_fiber_current(&model)->registers.ebx,
        2u);
    passed &= expect_u32(
        "select worker",
        recomp_fiber_set_current(&model, worker_handle),
        1u);
    passed &= expect_u32(
        "worker entry",
        recomp_fiber_current(&model)->entry,
        0x000b5570u);
    passed &= expect_u32(
        "worker parameter",
        recomp_fiber_current(&model)->parameter,
        0x005f2c38u);
    passed &= expect_u32(
        "worker ebx isolated",
        recomp_fiber_current(&model)->registers.ebx,
        7u);
    passed &= expect_u32(
        "cannot remove current",
        recomp_fiber_remove(&model, worker_handle),
        0u);
    passed &= expect_u32(
        "reselect main",
        recomp_fiber_set_current(&model, main_handle),
        1u);
    passed &= expect_u32(
        "remove worker",
        recomp_fiber_remove(&model, worker_handle),
        1u);
    passed &= expect_u32(
        "worker absent",
        recomp_fiber_find(&model, worker_handle) != NULL,
        0u);
    passed &= expect_u32(
        "unknown select rejected",
        recomp_fiber_set_current(&model, worker_handle),
        0u);

    return passed;
}
