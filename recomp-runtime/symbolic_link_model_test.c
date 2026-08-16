#include "symbolic_link_model.h"

#include <stdio.h>
#include <string.h>

int recomp_symbolic_link_model_test(void)
{
    RecompSymbolicLinkModel model;
    uint32_t handle = 0u;
    uint32_t second_handle = 0u;
    char target[RECOMP_SYMBOLIC_LINK_NAME_SIZE];
    int passed = 1;

    recomp_symbolic_link_reset(&model);
    passed &= !recomp_symbolic_link_open(&model, "Q:", &handle);
    passed &= recomp_symbolic_link_create(
        &model, "\\??\\D:", "\\Device\\CdRom0");
    passed &= recomp_symbolic_link_open(&model, "\\??\\d:", &handle);
    passed &= handle != 0u;
    passed &= recomp_symbolic_link_open(&model, "D:", &second_handle);
    passed &= second_handle != 0u;
    passed &= second_handle != handle;
    passed &= !recomp_symbolic_link_create(
        &model, "D:", "\\Device\\Harddisk0\\Partition1");
    passed &= recomp_symbolic_link_query(
        &model, handle, target, sizeof target);
    passed &= strcmp(target, "\\Device\\CdRom0") == 0;
    passed &= !recomp_symbolic_link_query(
        &model, handle, target, 4u);
    passed &= recomp_symbolic_link_close(&model, handle);
    passed &= !recomp_symbolic_link_query(
        &model, handle, target, sizeof target);
    passed &= recomp_symbolic_link_query(
        &model, second_handle, target, sizeof target);
    passed &= recomp_symbolic_link_resolve_path(
        &model, "d:\\media\\file.bin", target, sizeof target);
    passed &= strcmp(target, "\\Device\\CdRom0\\media\\file.bin") == 0;
    passed &= !recomp_symbolic_link_resolve_path(
        &model, "Doom:\\file.bin", target, sizeof target);
    passed &= recomp_symbolic_link_close(&model, second_handle);
    passed &= recomp_symbolic_link_open(&model, "\\??\\D:", &handle);
    passed &= recomp_symbolic_link_delete(&model, "D:");
    passed &= !recomp_symbolic_link_open(&model, "D:", &handle);

    if (!passed) {
        fprintf(stderr, "symbolic link model: case failed\n");
    }
    return passed;
}
