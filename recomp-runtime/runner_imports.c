#include "runner_imports.h"

#include "runtime.h"

static RecompImportState import_state;

static void record_first(RecompImportKind kind)
{
    if (import_state.first_import == RECOMP_IMPORT_NONE) {
        import_state.first_import = kind;
    }
}

void recomp_imports_reset(void)
{
    import_state = (RecompImportState){0};
}

const RecompImportState *recomp_imports_state(void)
{
    return &import_state;
}

void sub_00182DC7(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;

    record_first(RECOMP_IMPORT_CREATE_THREAD);
    ++import_state.create_thread_calls;
    import_state.thread_start = *recomp_memory_u32(call_esp + 12u);
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 28u;
}

void sub_00186396(void)
{
    record_first(RECOMP_IMPORT_BOOT_TO_DASH);
    ++import_state.boot_to_dash_calls;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 16u;
}

void sub_0018210D(void)
{
    record_first(RECOMP_IMPORT_EXIT_THREAD);
    ++import_state.exit_thread_calls;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 8u;
}
