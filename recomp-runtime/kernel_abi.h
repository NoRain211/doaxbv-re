#ifndef DOAXBV_RECOMP_KERNEL_ABI_H
#define DOAXBV_RECOMP_KERNEL_ABI_H

#include "runtime.h"

/* Shared calling-convention helpers for kernel import adapters.
 *
 * A kernel adapter is entered with the guest stack exactly as the CALL left
 * it: the return address at ESP, then the arguments. Adapters read their
 * arguments with kernel_arg() and finish with kernel_return(), which applies
 * the callee cleanup the XDK's stdcall imports expect.
 *
 * Check the convention before writing an adapter. Most xboxkrnl exports are
 * stdcall, the Kf* ones are fastcall, and DbgPrint is cdecl varargs. Getting
 * this wrong corrupts ESP and the failure appears somewhere else entirely.
 */

/* Argument 1 sits just past the return address. */
static inline uint32_t kernel_arg(unsigned index)
{
    return *recomp_memory_u32(
        recomp_runtime.registers.esp + 4u * index);
}

/* stdcall: pop the return address and the callee's arguments. */
static inline void kernel_return(unsigned argument_count, uint32_t result)
{
    recomp_runtime.registers.esp += 4u + 4u * argument_count;
    recomp_runtime.registers.eax = result;
}

/* cdecl and fastcall: the caller owns the arguments, so pop only the
   return address. Fastcall arguments arrive in ECX and EDX. */
static inline void kernel_return_caller_cleanup(uint32_t result)
{
    recomp_runtime.registers.esp += 4u;
    recomp_runtime.registers.eax = result;
}

/* Calls back into guest code with stdcall arguments. ESP is restored
   afterwards rather than trusted, because a generated callee that mismanages
   its own cleanup would otherwise corrupt the caller silently. */
static inline void kernel_call_guest(
    uint32_t guest_address,
    const uint32_t *arguments,
    unsigned count)
{
    uint32_t saved_esp = recomp_runtime.registers.esp;

    for (unsigned i = count; i > 0u; --i) {
        recomp_runtime.registers.esp -= 4u;
        *recomp_memory_u32(recomp_runtime.registers.esp) = arguments[i - 1u];
    }
    recomp_runtime.registers.esp -= 4u;
    *recomp_memory_u32(recomp_runtime.registers.esp) = 0u;
    recomp_dispatch_indirect(guest_address, saved_esp);
    recomp_runtime.registers.esp = saved_esp;
}

/* Resolves an encoded kernel thunk (0x8000xxxx) to its adapter. Declare this
   wherever it is called: an implicit declaration returns int and silently
   truncates the function pointer on a 64-bit host. */
RecompFunction recomp_lookup_kernel(uint32_t guest_address);

/* Filesystem root supplied by the runner for disc-file resolution. */
extern const char *recomp_disc_root_path;

/* Each subsystem exposes one of these; kernel_dispatch.c chains them. */
RecompFunction recomp_kernel_config(uint32_t ordinal);
RecompFunction recomp_kernel_debug(uint32_t ordinal);
RecompFunction recomp_kernel_device(uint32_t ordinal);
RecompFunction recomp_kernel_interrupt(uint32_t ordinal);
RecompFunction recomp_kernel_memory(uint32_t ordinal);
RecompFunction recomp_kernel_rtl(uint32_t ordinal);
RecompFunction recomp_kernel_startup(uint32_t ordinal);
RecompFunction recomp_kernel_thread(uint32_t ordinal);
RecompFunction recomp_kernel_video(uint32_t ordinal);
RecompFunction recomp_kernel_file(uint32_t ordinal);

/* Plain configuration model used by its import adapter. */
uint32_t recomp_kernel_query_nonvolatile_setting(
    uint32_t value_index,
    uint32_t type,
    uint32_t value,
    uint32_t value_length,
    uint32_t result_length);

/* Plain RTL models used by their import adapters. */
uint32_t recomp_kernel_ntstatus_to_dos_error(uint32_t status);

/* Plain image-section model used by its import adapter. */
uint32_t recomp_kernel_load_section(uint32_t section);

/* Plain AV model used by its import adapter. */
uint32_t recomp_kernel_av_get_saved_data_address(void);
uint32_t recomp_kernel_av_send_tv_encoder_option(
    uint32_t register_base,
    uint32_t option,
    uint32_t parameter,
    uint32_t result);

/* Plain guest allocation operations shared by kernel models. */
uint32_t recomp_kernel_allocate_pool(uint32_t size);
void recomp_kernel_free_pool(uint32_t base);

/* Plain DPC model operations shared by interrupt and timer adapters. */
uint32_t recomp_kernel_queue_dpc(
    uint32_t dpc,
    uint32_t argument1,
    uint32_t argument2);
uint32_t recomp_kernel_remove_dpc(uint32_t dpc);
void recomp_kernel_drain_dpcs(void);

#endif
