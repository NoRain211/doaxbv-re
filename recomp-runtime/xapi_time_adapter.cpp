#include "xapi_time_adapter.h"

#include <chrono>
#include <cstring>

uint64_t recomp_xapi_performance_counter(void)
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

uint64_t recomp_xapi_performance_frequency(void)
{
    return UINT64_C(1000000000);
}

namespace {

void return_time_value(uint64_t value)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t destination = *recomp_memory_u32(entry_esp + 4u);
    std::memcpy(recomp_memory(destination, sizeof(value)), &value, sizeof(value));
    recomp_runtime.registers.eax = 1u;
    recomp_runtime.registers.esp = entry_esp + 8u;
}

void query_performance_counter(void)
{
    return_time_value(recomp_xapi_performance_counter());
}

void query_performance_frequency(void)
{
    return_time_value(recomp_xapi_performance_frequency());
}

}

RecompFunction recomp_xapi_time_lookup_manual(uint32_t guest_address)
{
    switch (guest_address) {
    case 0x001830cfu:
        return query_performance_counter;
    case 0x001830e0u:
        return query_performance_frequency;
    default:
        return nullptr;
    }
}
