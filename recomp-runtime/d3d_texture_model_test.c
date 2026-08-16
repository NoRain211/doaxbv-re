#include "d3d_texture_adapter.h"
#include "program_manual.h"
#include "runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    TEST_STATIC_BASE = 0x001f0000u,
    TEST_STATIC_SIZE = 0x00007000u,
    TEST_CALL_BASE = 0x28000000u,
    TEST_CALL_SIZE = 0x00001000u,
    TEST_TEXTURE_BASE = 0x29000000u,
    TEST_TEXTURE_SIZE = 0x00001000u,
    TEST_BACKING_BASE = 0x021bf000u,
    TEST_BACKING_SIZE = 0x00001000u,
    TEST_ENTRY_ESP = TEST_CALL_BASE + 0x100u,
    TEST_DEVICE = 0x001f3120u,
};

static uint32_t release_call_count;
static uint32_t released_texture;
static uint32_t lock_call_count;
static uint32_t lock_address;

static void release_resource_stub(void)
{
    released_texture = *recomp_memory_u32(
        recomp_runtime.registers.esp + 4u);
    ++release_call_count;
    recomp_runtime.registers.esp += 8u;
}

void sub_001E8090(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t locked_rect = *recomp_memory_u32(entry_esp + 12u);

    *recomp_memory_u32(locked_rect) = 0x0b40u;
    *recomp_memory_u32(locked_rect + 4u) = lock_address;
    ++lock_call_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp = entry_esp + 24u;
}

static int expect_u32(const char *field, uint32_t actual, uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(
        stderr,
        "D3D texture adapter: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

static void prepare_call(uint32_t stage, uint32_t texture)
{
    *recomp_memory_u32(TEST_ENTRY_ESP + 4u) = stage;
    *recomp_memory_u32(TEST_ENTRY_ESP + 8u) = texture;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xa5a5a5a5u;
}

static void prepare_lock(uint32_t locked_rect)
{
    *recomp_memory_u32(TEST_ENTRY_ESP + 4u) = TEST_TEXTURE_BASE;
    *recomp_memory_u32(TEST_ENTRY_ESP + 8u) = 0u;
    *recomp_memory_u32(TEST_ENTRY_ESP + 12u) = locked_rect;
    *recomp_memory_u32(TEST_ENTRY_ESP + 16u) = 0u;
    *recomp_memory_u32(TEST_ENTRY_ESP + 20u) = 0u;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xa5a5a5a5u;
}

int recomp_d3d_texture_model_test(void)
{
    static uint8_t static_memory[TEST_STATIC_SIZE];
    static uint8_t call_memory[TEST_CALL_SIZE];
    static uint8_t texture_memory[TEST_TEXTURE_SIZE];
    static uint8_t backing_memory[TEST_BACKING_SIZE];
    const RecompMemoryRegion regions[] = {
        {TEST_STATIC_BASE, sizeof static_memory, static_memory},
        {TEST_CALL_BASE, sizeof call_memory, call_memory},
        {TEST_TEXTURE_BASE, sizeof texture_memory, texture_memory},
        {TEST_BACKING_BASE, sizeof backing_memory, backing_memory},
    };
    const RecompFunctionEntry functions[] = {
        {0x001e7c90u, release_resource_stub},
    };
    const RecompD3dTextureModel *model;
    RecompD3dTextureModel isolated = {0};
    RecompFunction adapter;
    RecompFunction lock_adapter;
    uint32_t resolved;
    int passed = 1;

    passed &= !recomp_d3d_set_texture(
        &isolated, RECOMP_D3D_TEXTURE_STAGE_COUNT, 0u);
    passed &= recomp_d3d_texture_resolve_cpu_address(
        0x021bf000u, &resolved);
    passed &= expect_u32("direct lock address", resolved, 0x021bf000u);
    passed &= recomp_d3d_texture_resolve_cpu_address(
        0x821bf000u, &resolved);
    passed &= expect_u32("cached lock address", resolved, 0x021bf000u);
    passed &= !recomp_d3d_texture_resolve_cpu_address(0u, &resolved);
    passed &= !recomp_d3d_texture_resolve_cpu_address(0x84000000u, &resolved);
    passed &= !recomp_d3d_texture_resolve_cpu_address(0x821bf000u, NULL);

    memset(static_memory, 0, sizeof static_memory);
    memset(call_memory, 0, sizeof call_memory);
    memset(texture_memory, 0, sizeof texture_memory);
    memset(backing_memory, 0, sizeof backing_memory);
    recomp_runtime_init(
        regions, 4u, NULL, 0u, functions, 1u);
    *recomp_memory_u32(0x001f2978u) = TEST_DEVICE;
    *recomp_memory_u32(0x001f2984u) = 0x100u;
    recomp_d3d_texture_adapter_reset();
    release_call_count = 0u;
    released_texture = 0u;
    lock_call_count = 0u;
    lock_address = 0u;
    model = recomp_d3d_texture_adapter_model();

    adapter = recomp_d3d_texture_lookup_manual(0x001e43f0u);
    if (adapter == NULL || recomp_lookup_manual(0x001e43f0u) != adapter ||
        recomp_d3d_texture_lookup_manual(0x001e43efu) != NULL ||
        recomp_d3d_texture_lookup_manual(0x001e43f1u) != NULL) {
        fprintf(stderr, "D3D texture adapter: lookup was not exact\n");
        return 0;
    }
    lock_adapter = recomp_d3d_texture_lookup_manual(0x001e8090u);
    if (lock_adapter == NULL ||
        recomp_lookup_manual(0x001e8090u) != lock_adapter ||
        recomp_d3d_texture_lookup_manual(0x001e808fu) != NULL ||
        recomp_d3d_texture_lookup_manual(0x001e8091u) != NULL) {
        fprintf(stderr, "D3D texture lock adapter: lookup was not exact\n");
        return 0;
    }

    lock_address = 0x821bf000u;
    prepare_lock(TEST_CALL_BASE + 0x200u);
    lock_adapter();
    passed &= expect_u32("lock call count", lock_call_count, 1u);
    passed &= expect_u32(
        "lock ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 24u);
    passed &= expect_u32("lock status", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32(
        "lock pitch", *recomp_memory_u32(TEST_CALL_BASE + 0x200u), 0x0b40u);
    passed &= expect_u32(
        "lock address",
        *recomp_memory_u32(TEST_CALL_BASE + 0x204u),
        0x021bf000u);
    *recomp_memory_u32(*recomp_memory_u32(TEST_CALL_BASE + 0x204u)) =
        0x11223344u;
    passed &= expect_u32(
        "lock backing write",
        *recomp_memory_u32(TEST_BACKING_BASE),
        0x11223344u);

    *recomp_memory_u32(TEST_TEXTURE_BASE) = 0x00100001u;
    *recomp_memory_u32(TEST_TEXTURE_BASE + 0x0cu) = 0x00002a34u;
    prepare_call(2u, TEST_TEXTURE_BASE);
    adapter();
    passed &= expect_u32("bind ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 12u);
    passed &= expect_u32("bind EAX", recomp_runtime.registers.eax, 0xa5a5a5a5u);
    passed &= expect_u32(
        "bound slot", *recomp_memory_u32(TEST_DEVICE + 0x0b40u),
        TEST_TEXTURE_BASE);
    passed &= expect_u32(
        "bound references", *recomp_memory_u32(TEST_TEXTURE_BASE),
        0x00180001u);
    passed &= expect_u32(
        "bound format", *recomp_memory_u32(TEST_DEVICE + 0x14u),
        0x40000034u);
    passed &= expect_u32(
        "bound dirty", *recomp_memory_u32(0x001f2984u), 0x00004900u);
    passed &= expect_u32("model stage", model->textures[2], TEST_TEXTURE_BASE);
    passed &= expect_u32("model updates", model->update_count, 1u);

    prepare_call(2u, 0u);
    adapter();
    passed &= expect_u32(
        "cleared slot", *recomp_memory_u32(TEST_DEVICE + 0x0b40u), 0u);
    passed &= expect_u32(
        "released references", *recomp_memory_u32(TEST_TEXTURE_BASE),
        0x00100001u);
    passed &= expect_u32(
        "release owner", *recomp_memory_u32(TEST_TEXTURE_BASE + 8u),
        TEST_DEVICE + 0x30u);
    passed &= expect_u32(
        "disabled format", *recomp_memory_u32(TEST_DEVICE + 0x14u),
        0x80000000u);
    passed &= expect_u32("model updates after clear", model->update_count, 2u);

    /* A reference count that reaches the generated release condition must
       continue through the resource-release function. */
    *recomp_memory_u32(TEST_TEXTURE_BASE + 0x100u) = 0u;
    *recomp_memory_u32(TEST_TEXTURE_BASE + 0x10cu) = 0x00000034u;
    prepare_call(3u, TEST_TEXTURE_BASE + 0x100u);
    adapter();
    prepare_call(3u, 0u);
    adapter();
    passed &= expect_u32("release call count", release_call_count, 1u);
    passed &= expect_u32(
        "released texture", released_texture, TEST_TEXTURE_BASE + 0x100u);
    passed &= expect_u32(
        "release ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 12u);

    /* Rebinding an equivalent format does not dirty the generated shadow. */
    *recomp_memory_u32(TEST_TEXTURE_BASE + 0x200u) = 0x00100001u;
    *recomp_memory_u32(TEST_TEXTURE_BASE + 0x20cu) = 0x00000034u;
    *recomp_memory_u32(TEST_DEVICE + 0x0cu) = 0x00000034u;
    *recomp_memory_u32(0x001f2984u) = 0x100u;
    prepare_call(0u, TEST_TEXTURE_BASE + 0x200u);
    adapter();
    passed &= expect_u32(
        "equivalent format dirty", *recomp_memory_u32(0x001f2984u), 0x100u);
    return passed;
}
