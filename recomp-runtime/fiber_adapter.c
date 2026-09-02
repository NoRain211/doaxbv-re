#include "fiber_adapter.h"
#include "stop_report.h"
#include "xbox_memory_layout.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    XAPI_CREATE_FIBER_ADDRESS = 0x00182fbbu,
    XAPI_DELETE_FIBER_ADDRESS = 0x00183047u,
    XAPI_SWITCH_TO_FIBER_ADDRESS = 0x0018305au,
    XAPI_CONVERT_THREAD_TO_FIBER_ADDRESS = 0x00183099u,
    XAPI_MINIMUM_FIBER_STACK = 0x3000u,
    NATIVE_FIBER_STACK_RESERVE = 0x100000u,
};

typedef struct RecompHostFiber {
    bool active;
    uint32_t guest_handle;
    uint32_t entry;
    uint64_t switch_count;
    LPVOID native_fiber;
} RecompHostFiber;

static RecompFiberModel fiber_model;
static RecompHostFiber host_fibers[RECOMP_FIBER_MAX_COUNT];
static RecompHostFiber *current_host_fiber;
static bool converted_thread;

static uint32_t main_fiber_handle(void)
{
    uint32_t tls_index = *recomp_memory_u32(0x003b5258u);
    uint32_t tls_slots = *recomp_memory_u32(4u);
    uint32_t tls_block = *recomp_memory_u32(tls_slots + tls_index * 4u);

    return tls_block + 8u;
}

static void publish_current_fiber(uint32_t guest_handle)
{
    uint32_t tls_index = *recomp_memory_u32(0x003b5258u);
    uint32_t tls_slots = *recomp_memory_u32(4u);
    uint32_t tls_block = *recomp_memory_u32(tls_slots + tls_index * 4u);

    *recomp_memory_u32(tls_block + 4u) = guest_handle;
}

static uint32_t stack_argument(uint32_t entry_esp, uint32_t index)
{
    return *recomp_memory_u32(entry_esp + 4u + index * 4u);
}

static void finish(uint32_t entry_esp, uint32_t argument_count, uint32_t result)
{
    recomp_runtime.registers.eax = result;
    recomp_runtime.registers.esp =
        entry_esp + 4u + argument_count * 4u;
}

static void fail_fiber(const char *reason, uint32_t guest_handle)
{
    fprintf(
        stderr,
        "recomp fiber: %s 0x%08" PRIx32 "\n",
        reason,
        guest_handle);
    recomp_stop(2, "fiber:%s:0x%08" PRIx32, reason, guest_handle);
}

static RecompHostFiber *host_fiber_find(uint32_t guest_handle)
{
    for (size_t i = 0u; i < RECOMP_FIBER_MAX_COUNT; ++i) {
        if (host_fibers[i].active &&
            host_fibers[i].guest_handle == guest_handle) {
            return &host_fibers[i];
        }
    }
    return NULL;
}

static RecompHostFiber *host_fiber_add(
    uint32_t guest_handle,
    LPVOID native_fiber)
{
    if (native_fiber == NULL || host_fiber_find(guest_handle) != NULL) {
        return NULL;
    }
    for (size_t i = 0u; i < RECOMP_FIBER_MAX_COUNT; ++i) {
        if (!host_fibers[i].active) {
            host_fibers[i] = (RecompHostFiber){
                .active = true,
                .guest_handle = guest_handle,
                .native_fiber = native_fiber,
            };
            return &host_fibers[i];
        }
    }
    return NULL;
}

static VOID WINAPI fiber_entry(void *parameter)
{
    RecompHostFiber *host = parameter;
    RecompFiber *fiber = recomp_fiber_find(&fiber_model, host->guest_handle);
    uint32_t entry;

    if (fiber == NULL || current_host_fiber != host ||
        fiber_model.current_handle != host->guest_handle) {
        fail_fiber("entry-context", host->guest_handle);
    }
    entry = fiber->entry;
    recomp_runtime.registers = fiber->registers;
    *recomp_memory_u32(0u) = fiber->exception_list;
    recomp_dispatch_indirect_site(
        entry, fiber->registers.esp, __FILE__, __LINE__);
    fail_fiber("entry-returned", host->guest_handle);
}

void recomp_fiber_adapter_reset(void)
{
    if (converted_thread && current_host_fiber != NULL &&
        current_host_fiber->guest_handle == main_fiber_handle()) {
        for (size_t i = 0u; i < RECOMP_FIBER_MAX_COUNT; ++i) {
            RecompHostFiber *host = &host_fibers[i];

            if (host->active && host != current_host_fiber) {
                DeleteFiber(host->native_fiber);
            }
        }
        (void)ConvertFiberToThread();
    }
    recomp_fiber_model_reset(&fiber_model);
    for (size_t i = 0u; i < RECOMP_FIBER_MAX_COUNT; ++i) {
        host_fibers[i] = (RecompHostFiber){0};
    }
    current_host_fiber = NULL;
    converted_thread = false;
}

const RecompFiberModel *recomp_fiber_adapter_model(void)
{
    return &fiber_model;
}

void recomp_fiber_adapter_report(void)
{
    for (size_t i = 0u; i < RECOMP_FIBER_MAX_COUNT; ++i) {
        const RecompHostFiber *host = &host_fibers[i];

        if (!host->active) {
            continue;
        }
        fprintf(
            stderr,
            "recomp fiber: handle=0x%08" PRIx32 " entry=0x%08" PRIx32
            " resumed=%llu%s\n",
            host->guest_handle,
            host->entry,
            (unsigned long long)host->switch_count,
            host == current_host_fiber ? " (current)" : "");
    }
}

static void convert_thread_to_fiber_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t fiber_data = stack_argument(entry_esp, 0u);
    uint32_t guest_handle = main_fiber_handle();
    RecompRegisters registers = recomp_runtime.registers;
    LPVOID native_fiber;
    RecompFiber *fiber;
    RecompHostFiber *host;

    if (converted_thread || current_host_fiber != NULL) {
        fail_fiber("duplicate-convert", guest_handle);
    }
    native_fiber = ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
    if (native_fiber == NULL) {
        fail_fiber("convert-failed", guest_handle);
    }
    fiber = recomp_fiber_add(
        &fiber_model,
        guest_handle,
        0u,
        fiber_data,
        &registers,
        *recomp_memory_u32(0u));
    host = host_fiber_add(guest_handle, native_fiber);
    if (fiber == NULL || host == NULL ||
        !recomp_fiber_set_current(&fiber_model, guest_handle)) {
        fail_fiber("convert-state", guest_handle);
    }

    *recomp_memory_u32(guest_handle) = fiber_data;
    *recomp_memory_u32(guest_handle + 4u) = entry_esp + 8u;
    *recomp_memory_u32(guest_handle + 8u) = 0u;
    *recomp_memory_u32(guest_handle + 12u) = entry_esp;
    publish_current_fiber(guest_handle);
    converted_thread = true;
    current_host_fiber = host;
    finish(entry_esp, 1u, guest_handle);
}

static void create_fiber_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t stack_size = stack_argument(entry_esp, 0u);
    uint32_t entry = stack_argument(entry_esp, 1u);
    uint32_t parameter = stack_argument(entry_esp, 2u);
    uint32_t stack_base;
    uint32_t stack_top;
    uint32_t guest_handle;
    uint32_t initial_esp;
    RecompRegisters registers = {0};
    RecompFiber *fiber;
    RecompHostFiber *host;
    LPVOID native_fiber;

    if (!converted_thread || entry == 0u) {
        fail_fiber("create-context", entry);
    }
    if (stack_size < XAPI_MINIMUM_FIBER_STACK) {
        stack_size = XAPI_MINIMUM_FIBER_STACK;
    }
    stack_size = (stack_size + 0xfffu) & 0xfffff000u;
    stack_base = xbox_HeapAlloc(stack_size, 0x1000u);
    if (stack_base == 0u || stack_base > UINT32_MAX - stack_size) {
        fail_fiber("guest-stack", entry);
    }
    stack_top = stack_base + stack_size;
    guest_handle = stack_top - 0x10u;
    initial_esp = guest_handle - 8u;
    registers.esp = initial_esp;

    *recomp_memory_u32(initial_esp) = 0u;
    *recomp_memory_u32(initial_esp + 4u) = parameter;
    *recomp_memory_u32(guest_handle) = parameter;
    *recomp_memory_u32(guest_handle + 4u) = stack_top;
    *recomp_memory_u32(guest_handle + 8u) = stack_base;
    *recomp_memory_u32(guest_handle + 12u) = initial_esp;

    fiber = recomp_fiber_add(
        &fiber_model,
        guest_handle,
        entry,
        parameter,
        &registers,
        0xffffffffu);
    if (fiber == NULL) {
        fail_fiber("create-state", guest_handle);
    }
    host = NULL;
    for (size_t i = 0u; i < RECOMP_FIBER_MAX_COUNT; ++i) {
        if (!host_fibers[i].active) {
            host = &host_fibers[i];
            break;
        }
    }
    if (host == NULL) {
        fail_fiber("create-host-capacity", guest_handle);
    }
    native_fiber = CreateFiberEx(
        0u,
        NATIVE_FIBER_STACK_RESERVE,
        FIBER_FLAG_FLOAT_SWITCH,
        fiber_entry,
        host);
    if (native_fiber == NULL) {
        fail_fiber("create-host", guest_handle);
    }
    *host = (RecompHostFiber){
        .active = true,
        .guest_handle = guest_handle,
        .entry = entry,
        .native_fiber = native_fiber,
    };
    fprintf(
        stderr,
        "recomp fiber: created handle=0x%08" PRIx32 " entry=0x%08" PRIx32
        " parameter=0x%08" PRIx32 "\n",
        guest_handle,
        entry,
        parameter);
    finish(entry_esp, 3u, guest_handle);
}

static void switch_to_fiber_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t result = recomp_runtime.registers.eax;
    uint32_t target_handle = stack_argument(entry_esp, 0u);
    RecompFiber *outgoing = recomp_fiber_current(&fiber_model);
    RecompFiber *target = recomp_fiber_find(&fiber_model, target_handle);
    RecompHostFiber *outgoing_host = current_host_fiber;
    RecompHostFiber *target_host = host_fiber_find(target_handle);

    if (outgoing == NULL || target == NULL || outgoing_host == NULL ||
        target_host == NULL) {
        fail_fiber("unknown-switch", target_handle);
    }
    if (target_handle == outgoing->guest_handle) {
        finish(entry_esp, 1u, result);
        return;
    }

    outgoing->registers = recomp_runtime.registers;
    outgoing->registers.esp = entry_esp + 8u;
    /* Hardware preserves the x87 and SSE context per thread. Measured at 240
       of 240 switches, this program's stack is non-empty at the boundary, so
       without this the incoming fiber inherits the outgoing one's floating
       point and silently computes with it. */
    recomp_fpu_context_save(&outgoing->fpu);
    /* Round 28 probe. RecompFiber carries only RecompRegisters, while the
       x87 stack, fpu_top and the XMM file live outside it in RecompRuntime
       and are therefore shared by every fiber. Real hardware gives each
       thread its own FP context. If fpu_top is ever non-zero here, live
       floating-point values are crossing a fiber boundary. Opt-in. */
    {
        static const char *fpu_trace;
        static bool fpu_trace_read;
        static uint32_t fpu_trace_lines;
        static uint32_t switches_seen;
        static uint32_t switches_with_stack;

        if (!fpu_trace_read) {
            fpu_trace_read = true;
            fpu_trace = getenv("RECOMP_FIBER_FPU");
        }
        if (fpu_trace != NULL) {
            ++switches_seen;
            if (recomp_runtime.fpu_top != 0u) {
                ++switches_with_stack;
            }
            if (fpu_trace_lines < 240u &&
                (recomp_runtime.fpu_top != 0u ||
                 (switches_seen % 500u) == 0u)) {
                ++fpu_trace_lines;
                fprintf(
                    stderr,
                    "recomp fiber fpu: switches=%" PRIu32
                    " nonempty=%" PRIu32 " top=%" PRIu32
                    " cmp=%d from=%08" PRIx32 " to=%08" PRIx32 "\n",
                    switches_seen,
                    switches_with_stack,
                    recomp_runtime.fpu_top,
                    recomp_runtime.fpu_compare,
                    outgoing->guest_handle,
                    target_handle);
            }
        }
    }
    ++target_host->switch_count;
    outgoing->exception_list = *recomp_memory_u32(0u);
    *recomp_memory_u32(outgoing->guest_handle + 12u) =
        outgoing->registers.esp;
    if (!recomp_fiber_set_current(&fiber_model, target_handle)) {
        fail_fiber("switch-state", target_handle);
    }
    recomp_runtime.registers = target->registers;
    recomp_fpu_context_restore(&target->fpu);
    *recomp_memory_u32(0u) = target->exception_list;
    publish_current_fiber(target_handle);
    current_host_fiber = target_host;
    SwitchToFiber(target_host->native_fiber);

    if (current_host_fiber != outgoing_host ||
        fiber_model.current_handle != outgoing->guest_handle) {
        fail_fiber("resume-context", outgoing->guest_handle);
    }
    recomp_runtime.registers.eax = result;
}

static void delete_fiber_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t guest_handle = stack_argument(entry_esp, 0u);
    RecompFiber *fiber = recomp_fiber_find(&fiber_model, guest_handle);
    RecompHostFiber *host = host_fiber_find(guest_handle);

    if (fiber == NULL || host == NULL || host == current_host_fiber) {
        fail_fiber("invalid-delete", guest_handle);
    }
    DeleteFiber(host->native_fiber);
    xbox_HeapFree(*recomp_memory_u32(guest_handle + 8u));
    *host = (RecompHostFiber){0};
    if (!recomp_fiber_remove(&fiber_model, guest_handle)) {
        fail_fiber("delete-state", guest_handle);
    }
    finish(entry_esp, 1u, 0u);
}

RecompFunction recomp_fiber_lookup_manual(uint32_t guest_address)
{
    switch (guest_address) {
    case XAPI_CREATE_FIBER_ADDRESS:
        return create_fiber_adapter;
    case XAPI_DELETE_FIBER_ADDRESS:
        return delete_fiber_adapter;
    case XAPI_SWITCH_TO_FIBER_ADDRESS:
        return switch_to_fiber_adapter;
    case XAPI_CONVERT_THREAD_TO_FIBER_ADDRESS:
        return convert_thread_to_fiber_adapter;
    default:
        return NULL;
    }
}
