#include "runtime.h"
#include "ohci_model.h"
#include "stop_report.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RecompRuntime recomp_runtime;
uint32_t recomp_last_dispatch_address;

enum {
    XBOX_RAM_SIZE = 0x04000000u,
    XBOX_CACHED_ALIAS = 0x80000000u,
    USB0_OHCI_BASE = 0xfed00000u,
};

typedef struct RecompMmioU32Access {
    bool pending;
    uint32_t guest_address;
    RecompOhciRegister reg;
    uint32_t original;
    uint32_t value;
} RecompMmioU32Access;

static RecompOhci usb0_ohci;
static RecompMmioU32Access mmio_u32_access;

void recomp_runtime_init(
    const RecompMemoryRegion *memory_regions,
    size_t memory_region_count,
    RecompMemoryAccess *accesses,
    size_t access_capacity,
    const RecompFunctionEntry *functions,
    size_t function_count)
{
    recomp_runtime = (RecompRuntime){
        .memory_regions = memory_regions,
        .memory_region_count = memory_region_count,
        .accesses = accesses,
        .access_capacity = access_capacity,
        .functions = functions,
        .function_count = function_count,
    };
    /* x87 reset default: all exceptions masked, round-to-nearest,
       extended precision. The guest CRT reads this back through FNSTCW. */
    recomp_runtime.fpu_control_word = 0x037fu;
    recomp_runtime.fpu_compare = 0;
    recomp_ohci_init(&usb0_ohci);
    mmio_u32_access = (RecompMmioU32Access){0};
}

void recomp_runtime_set_lookup(RecompFunctionLookup lookup)
{
    recomp_runtime.lookup = lookup;
}

static void fail_memory_access(uint32_t guest_address, size_t width)
{
    ++recomp_runtime.undeclared_access_count;
    fprintf(
        stderr,
        "recomp runtime: undeclared %zu-byte guest memory access at 0x%08"
        PRIx32 "\n",
        width,
        guest_address);
    recomp_stop(2, "memory:0x%08" PRIx32, guest_address);
}

static void record_memory_access(uint32_t guest_address, size_t width)
{
    if (recomp_runtime.accesses == NULL) {
        return;
    }
    if (recomp_runtime.access_count >= recomp_runtime.access_capacity) {
        fprintf(stderr, "recomp runtime: memory access log full\n");
        recomp_stop(2, "access-log-full");
    }

    recomp_runtime.accesses[recomp_runtime.access_count++] =
        (RecompMemoryAccess){
            .address = guest_address,
            .width = (uint32_t)width,
        };
}

/* MEM32 is an lvalue. Commit a changed staging word on the next memory access
   so device write semantics stay in the model rather than in interception. */
static void commit_mmio_u32_write(void)
{
    if (!mmio_u32_access.pending) {
        return;
    }

    mmio_u32_access.pending = false;
    if (mmio_u32_access.value == mmio_u32_access.original) {
        return;
    }
    if (!recomp_ohci_write(
            &usb0_ohci,
            mmio_u32_access.reg,
            mmio_u32_access.value)) {
        fprintf(
            stderr,
            "recomp runtime: unsupported OHCI write 0x%08" PRIx32
            " at 0x%08" PRIx32 "\n",
            mmio_u32_access.value,
            mmio_u32_access.guest_address);
        recomp_stop(
            2, "ohci-write:0x%08" PRIx32, mmio_u32_access.guest_address);
    }
}

static bool usb0_ohci_register(
    uint32_t guest_address,
    RecompOhciRegister *reg)
{
    switch (guest_address) {
    case USB0_OHCI_BASE + 0x00u:
        *reg = RECOMP_OHCI_HC_REVISION;
        return true;
    case USB0_OHCI_BASE + 0x04u:
        *reg = RECOMP_OHCI_HC_CONTROL;
        return true;
    case USB0_OHCI_BASE + 0x08u:
        *reg = RECOMP_OHCI_HC_COMMAND_STATUS;
        return true;
    case USB0_OHCI_BASE + 0x10u:
        *reg = RECOMP_OHCI_HC_INTERRUPT_ENABLE;
        return true;
    case USB0_OHCI_BASE + 0x18u:
        *reg = RECOMP_OHCI_HC_HCCA;
        return true;
    case USB0_OHCI_BASE + 0x1cu:
        *reg = RECOMP_OHCI_HC_PERIOD_CURRENT_ED;
        return true;
    case USB0_OHCI_BASE + 0x20u:
        *reg = RECOMP_OHCI_HC_CONTROL_HEAD_ED;
        return true;
    case USB0_OHCI_BASE + 0x24u:
        *reg = RECOMP_OHCI_HC_CONTROL_CURRENT_ED;
        return true;
    case USB0_OHCI_BASE + 0x28u:
        *reg = RECOMP_OHCI_HC_BULK_HEAD_ED;
        return true;
    case USB0_OHCI_BASE + 0x2cu:
        *reg = RECOMP_OHCI_HC_BULK_CURRENT_ED;
        return true;
    case USB0_OHCI_BASE + 0x30u:
        *reg = RECOMP_OHCI_HC_DONE_HEAD;
        return true;
    case USB0_OHCI_BASE + 0x34u:
        *reg = RECOMP_OHCI_HC_FM_INTERVAL;
        return true;
    case USB0_OHCI_BASE + 0x40u:
        *reg = RECOMP_OHCI_HC_PERIODIC_START;
        return true;
    case USB0_OHCI_BASE + 0x44u:
        *reg = RECOMP_OHCI_HC_LS_THRESHOLD;
        return true;
    case USB0_OHCI_BASE + 0x48u:
        *reg = RECOMP_OHCI_HC_RH_DESCRIPTOR_A;
        return true;
    case USB0_OHCI_BASE + 0x4cu:
        *reg = RECOMP_OHCI_HC_RH_DESCRIPTOR_B;
        return true;
    case USB0_OHCI_BASE + 0x50u:
        *reg = RECOMP_OHCI_HC_RH_STATUS;
        return true;
    case USB0_OHCI_BASE + 0x54u:
        *reg = RECOMP_OHCI_HC_RH_PORT_STATUS_1;
        return true;
    case USB0_OHCI_BASE + 0x58u:
        *reg = RECOMP_OHCI_HC_RH_PORT_STATUS_2;
        return true;
    case USB0_OHCI_BASE + 0x5cu:
        *reg = RECOMP_OHCI_HC_RH_PORT_STATUS_3;
        return true;
    case USB0_OHCI_BASE + 0x60u:
        *reg = RECOMP_OHCI_HC_RH_PORT_STATUS_4;
        return true;
    default:
        return false;
    }
}

static uint8_t *find_memory_region(uint32_t guest_address, size_t width)
{
    uint64_t access_end = (uint64_t)guest_address + width;

    for (size_t i = 0; i < recomp_runtime.memory_region_count; ++i) {
        const RecompMemoryRegion *region = &recomp_runtime.memory_regions[i];
        uint64_t region_end = (uint64_t)region->address + region->size;

        if (region->data != NULL &&
            guest_address >= region->address &&
            access_end <= region_end) {
            return region->data + (guest_address - region->address);
        }
    }

    return NULL;
}

static uint8_t *recomp_memory(uint32_t guest_address, size_t width)
{
    uint8_t *memory;

    commit_mmio_u32_write();

    memory = find_memory_region(guest_address, width);
    if (memory == NULL && guest_address >= XBOX_CACHED_ALIAS &&
        guest_address - XBOX_CACHED_ALIAS < XBOX_RAM_SIZE) {
        memory = find_memory_region(
            guest_address - XBOX_CACHED_ALIAS, width);
    }
    if (memory != NULL) {
        record_memory_access(guest_address, width);
        return memory;
    }

    fail_memory_access(guest_address, width);
    return NULL;
}

uint32_t *recomp_memory_u32(uint32_t guest_address)
{
    RecompOhciRegister reg;
    uint32_t value;

    commit_mmio_u32_write();
    if (usb0_ohci_register(guest_address, &reg)) {
        if (!recomp_ohci_read(&usb0_ohci, reg, &value)) {
            fail_memory_access(guest_address, sizeof(uint32_t));
            return NULL;
        }
        record_memory_access(guest_address, sizeof(uint32_t));
        mmio_u32_access = (RecompMmioU32Access){
            .pending = true,
            .guest_address = guest_address,
            .reg = reg,
            .original = value,
            .value = value,
        };
        return &mmio_u32_access.value;
    }

    return (uint32_t *)(void *)recomp_memory(
        guest_address, sizeof(uint32_t));
}

uint64_t *recomp_memory_u64(uint32_t guest_address)
{
    return (uint64_t *)(void *)recomp_memory(
        guest_address, sizeof(uint64_t));
}

uint16_t *recomp_memory_u16(uint32_t guest_address)
{
    return (uint16_t *)(void *)recomp_memory(
        guest_address, sizeof(uint16_t));
}

int8_t *recomp_memory_i8(uint32_t guest_address)
{
    return (int8_t *)(void *)recomp_memory(guest_address, sizeof(int8_t));
}

void recomp_guest_memcpy(
    uint32_t destination,
    uint32_t source,
    size_t size)
{
    uint8_t *destination_data;
    const uint8_t *source_data;

    if (size == 0u) {
        return;
    }

    source_data = recomp_memory(source, size);
    destination_data = recomp_memory(destination, size);
    memcpy(destination_data, source_data, size);
}

void recomp_guest_memset(
    uint32_t destination,
    int value,
    size_t size)
{
    uint8_t *destination_data;

    if (size == 0u) {
        return;
    }

    destination_data = recomp_memory(destination, size);
    memset(destination_data, value, size);
}

uint32_t *recomp_ebp_register(void)
{
    return &recomp_runtime.registers.ebp;
}

uint32_t *recomp_esp_register(void)
{
    return &recomp_runtime.registers.esp;
}

bool recomp_dispatch(uint32_t guest_address)
{
    /* ponytail: linear lookup; sort and binary-search when table size matters. */
    for (size_t i = 0; i < recomp_runtime.function_count; ++i) {
        const RecompFunctionEntry *entry = &recomp_runtime.functions[i];

        if (entry->address == guest_address && entry->function != NULL) {
            recomp_last_dispatch_address = guest_address;
            entry->function();
            return true;
        }
    }

    if (recomp_runtime.lookup != NULL) {
        RecompFunction function = recomp_runtime.lookup(guest_address);

        if (function != NULL) {
            recomp_last_dispatch_address = guest_address;
            function();
            return true;
        }
    }

    return false;
}

static const char *member_name(const char *member)
{
    const char *name = member;

    for (const char *cursor = member; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            name = cursor + 1;
        }
    }
    return name;
}

static void copy_token(char *out, size_t size, const char *text)
{
    size_t i = 0;

    while (i + 1u < size && text[i] != '\0' && text[i] != ':' &&
           text[i] != '(' && text[i] != ' ' && text[i] != '\r' &&
           text[i] != '\n') {
        out[i] = text[i];
        ++i;
    }
    out[i] = '\0';
}

/* Names the generated site by rescanning the compiled member for the last
   guest label above the reported line; generated members carry no line map. */
static void generated_site(
    const char *member,
    int line,
    char *function,
    size_t function_size,
    char *label,
    size_t label_size)
{
    FILE *source = fopen(member, "rb");
    char text[512];
    int line_number = 1;
    int at_line_start = 1;

    function[0] = '\0';
    label[0] = '\0';
    if (source == NULL) {
        return;
    }

    while (line_number <= line && fgets(text, sizeof text, source) != NULL) {
        size_t length;

        if (at_line_start) {
            if (strncmp(text, "loc_", 4) == 0) {
                copy_token(label, label_size, text);
            } else if (strncmp(text, "void sub_", 9) == 0) {
                copy_token(function, function_size, text + 5);
                label[0] = '\0';
            }
        }
        length = strlen(text);
        at_line_start = length > 0u && text[length - 1u] == '\n';
        if (at_line_start) {
            ++line_number;
        }
    }
    fclose(source);
}

void recomp_generated_breakpoint(const char *member, int line)
{
    char function[64];
    char label[64];

    generated_site(member, line, function, sizeof function, label, sizeof label);
    fprintf(
        stderr,
        "recomp runtime: generated breakpoint at %s:%d function=%s label=%s"
        " esp=0x%08" PRIx32 " ebp=0x%08" PRIx32 " dispatch=0x%08" PRIx32 "\n",
        member_name(member),
        line,
        function[0] != '\0' ? function : "<unknown>",
        label[0] != '\0' ? label : "<unknown>",
        recomp_runtime.registers.esp,
        recomp_runtime.registers.ebp,
        recomp_last_dispatch_address);
    recomp_stop(2, "breakpoint:%s", member_name(member));
}

void recomp_dispatch_indirect(uint32_t guest_address, uint32_t saved_esp)
{
    if (recomp_dispatch(guest_address)) {
        return;
    }

    /* Encoded kernel thunks read as 0x8000xxxx; name the missing import. */
    if ((guest_address & 0xffff0000u) == 0x80000000u) {
        uint32_t ordinal = guest_address & 0xffffu;
        const char *name = recomp_kernel_ordinal_name(ordinal);

        fprintf(
            stderr,
            "recomp runtime: unimplemented kernel import ordinal %" PRIu32
            " (%s) at saved esp 0x%08" PRIx32 "\n",
            ordinal,
            name != NULL ? name : "not imported by this XBE",
            saved_esp);
        recomp_stop(
            2, "import:%s", name != NULL ? name : "unknown");
    }

    fprintf(
        stderr,
        "recomp runtime: unresolved indirect call to 0x%08" PRIx32
        " at saved esp 0x%08" PRIx32 "\n",
        guest_address,
        saved_esp);
    recomp_stop(2, "indirect:0x%08" PRIx32, guest_address);
}
