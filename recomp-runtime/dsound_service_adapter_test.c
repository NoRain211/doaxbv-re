#include "dsound_service_adapter.h"
#include "program_manual.h"
#include "runtime.h"
#include "xbox_memory_layout.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    TEST_STATIC_BASE = 0x00210000u,
    TEST_STATIC_SIZE = 0x00090000u,
    TEST_CALL_BASE = 0x28000000u,
    TEST_CALL_SIZE = 0x00001000u,
    TEST_HEAP_BASE = 0x27000000u,
    TEST_HEAP_SIZE = 0x00001000u,
    TEST_ENTRY_ESP = TEST_CALL_BASE + 0x100u,
    TEST_OUTPUT = TEST_CALL_BASE + 0x200u,
};

void recomp_test_heap_reset(uint32_t cursor, int fail_after);

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
        "DirectSound service adapter: %s was 0x%08x, expected 0x%08x\n",
        field,
        actual,
        expected);
    return 0;
}

static uint32_t float_bits(float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static void write_argument(uint32_t index, uint32_t value)
{
    *recomp_memory_u32(TEST_ENTRY_ESP + 4u + index * 4u) = value;
}

static int expect_lookup(uint32_t address)
{
    RecompFunction adapter = recomp_dsound_service_lookup_manual(address);

    if (adapter != NULL && recomp_lookup_manual(address) == adapter) {
        return 1;
    }
    fprintf(
        stderr,
        "DirectSound service adapter: lookup failed for 0x%08x\n",
        address);
    return 0;
}

int recomp_dsound_service_adapter_test(void)
{
    static uint8_t static_memory[TEST_STATIC_SIZE];
    static uint8_t call_memory[TEST_CALL_SIZE];
    static uint8_t heap_memory[TEST_HEAP_SIZE];
    const RecompMemoryRegion regions[] = {
        {
            .address = TEST_STATIC_BASE,
            .size = sizeof static_memory,
            .data = static_memory,
        },
        {
            .address = TEST_CALL_BASE,
            .size = sizeof call_memory,
            .data = call_memory,
        },
        {
            .address = TEST_HEAP_BASE,
            .size = sizeof heap_memory,
            .data = heap_memory,
        },
    };
    const RecompDsoundServiceModel *model;
    RecompFunction adapter;
    int passed = 1;

    memset(static_memory, 0, sizeof static_memory);
    memset(call_memory, 0, sizeof call_memory);
    memset(heap_memory, 0xa5, sizeof heap_memory);
    recomp_runtime_init(regions, 3u, NULL, 0u, NULL, 0u);
    recomp_test_heap_reset(TEST_HEAP_BASE, -1);
    recomp_dsound_service_adapter_reset();
    model = recomp_dsound_service_adapter_model();
    passed &= expect_u32("reset work count", model->work_count, 0u);
    passed &= expect_u32("reset commit count", model->commit_count, 0u);
    passed &= expect_u32(
        "reset mix-bin headroom count", model->mix_bin_headroom_count, 0u);
    passed &= expect_lookup(0x001f90e0u);
    passed &= expect_lookup(0x001f8f21u);
    passed &= expect_lookup(0x001f8f48u);
    passed &= expect_lookup(0x001fa27cu);
    passed &= expect_lookup(0x001f974fu);
    passed &= expect_lookup(0x001f9dd4u);
    passed &= expect_lookup(0x001f9e09u);
    if (recomp_dsound_service_lookup_manual(0x001fa27bu) != NULL ||
        recomp_dsound_service_lookup_manual(0x001fa27du) != NULL ||
        recomp_dsound_service_lookup_manual(0x001f8f20u) != NULL ||
        recomp_dsound_service_lookup_manual(0x001f8f22u) != NULL ||
        recomp_dsound_service_lookup_manual(0x001f8f47u) != NULL ||
        recomp_dsound_service_lookup_manual(0x001f8f49u) != NULL ||
        recomp_dsound_service_lookup_manual(0x001f90dfu) != NULL ||
        recomp_dsound_service_lookup_manual(0x001f9e3eu) != NULL) {
        fprintf(stderr, "DirectSound service adapter: lookup was not exact\n");
        return 0;
    }

    write_argument(0u, 0u);
    write_argument(1u, TEST_OUTPUT);
    write_argument(2u, 0u);
    *recomp_memory_u32(TEST_OUTPUT) = 0xa5a5a5a5u;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    adapter = recomp_dsound_service_lookup_manual(0x001fa27cu);
    adapter();
    passed &= expect_u32(
        "create ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 16u);
    passed &= expect_u32(
        "create HRESULT", recomp_runtime.registers.eax, RECOMP_DSOUND_OK);
    passed &= expect_u32(
        "create output", *recomp_memory_u32(TEST_OUTPUT),
        TEST_HEAP_BASE + 8u);
    passed &= expect_u32(
        "manager vtable", *recomp_memory_u32(TEST_HEAP_BASE), 0x00239decu);
    passed &= expect_u32(
        "manager references", *recomp_memory_u32(TEST_HEAP_BASE + 4u), 2u);
    passed &= expect_u32(
        "manager device", *recomp_memory_u32(TEST_HEAP_BASE + 8u),
        TEST_HEAP_BASE + 0x30u);
    passed &= expect_u32(
        "manager list head", *recomp_memory_u32(TEST_HEAP_BASE + 0x10u),
        TEST_HEAP_BASE + 0x10u);
    passed &= expect_u32(
        "manager list tail", *recomp_memory_u32(TEST_HEAP_BASE + 0x14u),
        TEST_HEAP_BASE + 0x10u);
    passed &= expect_u32(
        "manager global", *recomp_memory_u32(0x00214708u), TEST_HEAP_BASE);
    passed &= expect_u32("model manager", model->manager, TEST_HEAP_BASE);
    passed &= expect_u32(
        "device vtable", *recomp_memory_u32(TEST_HEAP_BASE + 0x30u),
        0x00239e1cu);
    passed &= expect_u32(
        "device references", *recomp_memory_u32(TEST_HEAP_BASE + 0x34u),
        1u);
    passed &= expect_u32(
        "model device", model->device, TEST_HEAP_BASE + 0x30u);

    write_argument(0u, TEST_HEAP_BASE + 8u);
    write_argument(1u, TEST_CALL_BASE + 0x300u);
    write_argument(2u, 18812u);
    write_argument(3u, TEST_CALL_BASE + 0x380u);
    write_argument(4u, TEST_OUTPUT);
    *recomp_memory_u32(TEST_OUTPUT) = 0xa5a5a5a5u;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    adapter = recomp_dsound_service_lookup_manual(0x001f8f21u);
    adapter();
    passed &= expect_u32(
        "download effects ESP", recomp_runtime.registers.esp,
        TEST_ENTRY_ESP + 24u);
    passed &= expect_u32(
        "download effects HRESULT", recomp_runtime.registers.eax,
        RECOMP_DSOUND_OK);
    passed &= expect_u32(
        "download effects descriptor", *recomp_memory_u32(TEST_OUTPUT), 0u);
    passed &= expect_u32(
        "download effects buffer", model->effects_image_buffer,
        TEST_CALL_BASE + 0x300u);
    passed &= expect_u32(
        "download effects size", model->effects_image_size, 18812u);
    passed &= expect_u32(
        "download effects location", model->effects_image_location,
        TEST_CALL_BASE + 0x380u);
    passed &= expect_u32(
        "download effects count", model->effects_image_download_count, 1u);

    write_argument(0u, TEST_HEAP_BASE + 8u);
    write_argument(1u, 10u);
    write_argument(2u, 0u);
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xffffffffu;
    adapter = recomp_dsound_service_lookup_manual(0x001f8f48u);
    adapter();
    passed &= expect_u32(
        "mix-bin headroom ESP", recomp_runtime.registers.esp,
        TEST_ENTRY_ESP + 16u);
    passed &= expect_u32(
        "mix-bin headroom HRESULT", recomp_runtime.registers.eax,
        RECOMP_DSOUND_OK);
    passed &= expect_u32(
        "mix-bin headroom bin", model->mix_bin, 10u);
    passed &= expect_u32(
        "mix-bin headroom value", model->mix_bin_headroom, 0u);
    passed &= expect_u32(
        "mix-bin headroom count", model->mix_bin_headroom_count, 1u);

    recomp_dsound_service_adapter_reset();
    recomp_test_heap_reset(TEST_HEAP_BASE + 0x100u, 1);
    adapter = recomp_dsound_service_lookup_manual(0x001fa27cu);
    write_argument(1u, TEST_OUTPUT);
    *recomp_memory_u32(TEST_OUTPUT) = 0xa5a5a5a5u;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    adapter();
    passed &= expect_u32(
        "device allocation failure HRESULT", recomp_runtime.registers.eax,
        RECOMP_DSOUND_OUT_OF_MEMORY);
    passed &= expect_u32(
        "device allocation failure output",
        *recomp_memory_u32(TEST_OUTPUT), 0u);
    passed &= expect_u32(
        "device allocation rollback", xbox_HeapCheckpoint(),
        TEST_HEAP_BASE + 0x100u);

    recomp_dsound_service_adapter_reset();
    recomp_test_heap_reset(TEST_HEAP_BASE + 0x200u, 0);
    *recomp_memory_u32(TEST_OUTPUT) = 0xa5a5a5a5u;
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    adapter();
    passed &= expect_u32(
        "manager allocation failure HRESULT", recomp_runtime.registers.eax,
        RECOMP_DSOUND_OUT_OF_MEMORY);
    passed &= expect_u32(
        "manager allocation failure output",
        *recomp_memory_u32(TEST_OUTPUT), 0u);
    passed &= expect_u32(
        "manager allocation failure heap", xbox_HeapCheckpoint(),
        TEST_HEAP_BASE + 0x200u);

    recomp_dsound_service_adapter_reset();
    recomp_test_heap_reset(TEST_HEAP_BASE + 0x300u, -1);
    write_argument(1u, 0u);
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    adapter();
    passed &= expect_u32(
        "null output HRESULT", recomp_runtime.registers.eax,
        RECOMP_DSOUND_POINTER_ERROR);
    passed &= expect_u32(
        "null output ESP", recomp_runtime.registers.esp,
        TEST_ENTRY_ESP + 16u);

    recomp_dsound_service_adapter_reset();
    recomp_test_heap_reset(TEST_HEAP_BASE, -1);

    write_argument(0u, 8u);
    write_argument(1u, float_bits(1.25f));
    write_argument(2u, float_bits(-2.5f));
    write_argument(3u, float_bits(3.75f));
    write_argument(4u, 1u);
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xffffffffu;
    adapter = recomp_dsound_service_lookup_manual(0x001f9dd4u);
    adapter();
    passed &= expect_u32(
        "position ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 24u);
    passed &= expect_u32("position HRESULT", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32(
        "position x", float_bits(model->listener_position.x),
        float_bits(1.25f));
    passed &= expect_u32(
        "position y", float_bits(model->listener_position.y),
        float_bits(-2.5f));
    passed &= expect_u32(
        "position z", float_bits(model->listener_position.z),
        float_bits(3.75f));
    passed &= expect_u32("position apply", model->position_apply, 1u);

    write_argument(1u, float_bits(-4.0f));
    write_argument(2u, float_bits(5.5f));
    write_argument(3u, float_bits(6.0f));
    write_argument(4u, 2u);
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xffffffffu;
    adapter = recomp_dsound_service_lookup_manual(0x001f9e09u);
    adapter();
    passed &= expect_u32(
        "velocity ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 24u);
    passed &= expect_u32("velocity HRESULT", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32(
        "velocity x", float_bits(model->listener_velocity.x),
        float_bits(-4.0f));
    passed &= expect_u32(
        "velocity y", float_bits(model->listener_velocity.y),
        float_bits(5.5f));
    passed &= expect_u32(
        "velocity z", float_bits(model->listener_velocity.z),
        float_bits(6.0f));
    passed &= expect_u32("velocity apply", model->velocity_apply, 2u);

    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xffffffffu;
    adapter = recomp_dsound_service_lookup_manual(0x001f974fu);
    adapter();
    passed &= expect_u32(
        "commit ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 8u);
    passed &= expect_u32("commit HRESULT", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32("commit count", model->commit_count, 1u);

    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_runtime.registers.eax = 0xa5a5a5a5u;
    adapter = recomp_dsound_service_lookup_manual(0x001f90e0u);
    adapter();
    passed &= expect_u32(
        "work ESP", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 4u);
    passed &= expect_u32(
        "work preserved EAX", recomp_runtime.registers.eax, 0xa5a5a5a5u);
    passed &= expect_u32("work count", model->work_count, 1u);

    return passed;
}
