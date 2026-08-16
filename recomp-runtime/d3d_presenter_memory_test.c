#include "d3d_presenter_memory_test.h"

#include <stdio.h>

static int expect_u32(
    const char *field,
    uint32_t actual,
    uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "D3D memory presenter: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

int recomp_d3d_presenter_memory_test(void)
{
    const RecompD3dPresenterConfig config = {
        .width = 720u,
        .height = 480u,
        .color_format = RECOMP_D3D_PRESENTER_COLOR_FORMAT_BGRA8_UNORM,
        .depth_format = RECOMP_D3D_PRESENTER_DEPTH_FORMAT_D24S8,
    };
    const RecompD3dPresenterCommand clear = {
        .type = RECOMP_D3D_PRESENTER_COMMAND_CLEAR,
        .data.clear = {
            .clear_color = true,
            .clear_depth = true,
            .clear_stencil = true,
            .color = 0x10203040u,
            .z = 1.0f,
            .stencil = 7u,
        },
    };
    const RecompD3dPresenterCommand present = {
        .type = RECOMP_D3D_PRESENTER_COMMAND_PRESENT,
        .data.present = {
            .effective_flags = 5u,
            .swap_counter = 1u,
        },
    };
    RecompD3dPresenterMemorySnapshot snapshot;
    RecompD3dPresenter *presenter = NULL;
    RecompD3dPresenterCommand unsupported = present;
    RecompD3dPresenterConfig invalid_config = config;
    int passed = 1;

    passed &= expect_u32(
        "submit before create",
        recomp_d3d_presenter_submit(NULL, &clear),
        RECOMP_D3D_PRESENTER_NOT_INITIALIZED);
    passed &= expect_u32(
        "destroy before create",
        recomp_d3d_presenter_destroy(&presenter),
        RECOMP_D3D_PRESENTER_NOT_INITIALIZED);
    invalid_config.color_format =
        RECOMP_D3D_PRESENTER_COLOR_FORMAT_UNKNOWN;
    passed &= expect_u32(
        "unknown color format",
        recomp_d3d_presenter_create(&invalid_config, &presenter),
        RECOMP_D3D_PRESENTER_INVALID_ARGUMENT);
    invalid_config = config;
    invalid_config.depth_format =
        RECOMP_D3D_PRESENTER_DEPTH_FORMAT_UNKNOWN;
    passed &= expect_u32(
        "unknown depth format",
        recomp_d3d_presenter_create(&invalid_config, &presenter),
        RECOMP_D3D_PRESENTER_INVALID_ARGUMENT);
    passed &= expect_u32(
        "create",
        recomp_d3d_presenter_create(&config, &presenter),
        RECOMP_D3D_PRESENTER_OK);
    passed &= expect_u32(
        "repeat create",
        recomp_d3d_presenter_create(&config, &presenter),
        RECOMP_D3D_PRESENTER_ALREADY_INITIALIZED);
    if (!recomp_d3d_presenter_memory_snapshot(&snapshot)) {
        fprintf(stderr, "D3D memory presenter: create snapshot unavailable\n");
        passed = 0;
    } else {
        passed &= expect_u32("width", snapshot.config.width, 720u);
        passed &= expect_u32("height", snapshot.config.height, 480u);
        passed &= expect_u32(
            "color format",
            snapshot.config.color_format,
            RECOMP_D3D_PRESENTER_COLOR_FORMAT_BGRA8_UNORM);
        passed &= expect_u32(
            "depth format",
            snapshot.config.depth_format,
            RECOMP_D3D_PRESENTER_DEPTH_FORMAT_D24S8);
        passed &= expect_u32("initial command count", snapshot.command_count, 0u);
    }

    passed &= expect_u32(
        "submit clear",
        recomp_d3d_presenter_submit(presenter, &clear),
        RECOMP_D3D_PRESENTER_OK);
    passed &= expect_u32(
        "submit present",
        recomp_d3d_presenter_submit(presenter, &present),
        RECOMP_D3D_PRESENTER_OK);
    passed &= expect_u32(
        "submit null command",
        recomp_d3d_presenter_submit(presenter, NULL),
        RECOMP_D3D_PRESENTER_INVALID_ARGUMENT);
    unsupported.type = (RecompD3dPresenterCommandType)99;
    passed &= expect_u32(
        "submit unsupported command",
        recomp_d3d_presenter_submit(presenter, &unsupported),
        RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND);
    if (!recomp_d3d_presenter_memory_snapshot(&snapshot)) {
        fprintf(stderr, "D3D memory presenter: command snapshot unavailable\n");
        passed = 0;
    } else {
        passed &= expect_u32("command count", snapshot.command_count, 2u);
        passed &= expect_u32("clear count", snapshot.clear_count, 1u);
        passed &= expect_u32("present count", snapshot.present_count, 1u);
        passed &= expect_u32(
            "snapshot clear type",
            snapshot.commands[0].type,
            RECOMP_D3D_PRESENTER_COMMAND_CLEAR);
        passed &= expect_u32(
            "snapshot clear color",
            snapshot.commands[0].data.clear.color,
            0x10203040u);
        passed &= expect_u32(
            "snapshot present type",
            snapshot.commands[1].type,
            RECOMP_D3D_PRESENTER_COMMAND_PRESENT);
        passed &= expect_u32(
            "snapshot present counter",
            snapshot.commands[1].data.present.swap_counter,
            1u);
    }

    passed &= expect_u32(
        "destroy",
        recomp_d3d_presenter_destroy(&presenter),
        RECOMP_D3D_PRESENTER_OK);
    if (presenter != NULL) {
        fprintf(stderr, "D3D memory presenter: destroy left a presenter\n");
        passed = 0;
    }
    if (recomp_d3d_presenter_memory_snapshot(&snapshot)) {
        fprintf(stderr, "D3D memory presenter: snapshot survived destroy\n");
        passed = 0;
    }
    passed &= expect_u32(
        "repeat destroy",
        recomp_d3d_presenter_destroy(&presenter),
        RECOMP_D3D_PRESENTER_NOT_INITIALIZED);

    return passed;
}
