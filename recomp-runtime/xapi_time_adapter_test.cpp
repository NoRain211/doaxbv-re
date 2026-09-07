#include "xapi_time_adapter.h"

extern "C" {
#include "program_manual.h"
}

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace {

constexpr uint32_t memory_base = 0x29000000u;
constexpr size_t memory_size = 256u;
constexpr size_t guard_size = 8u;
constexpr uint32_t entry_esp = memory_base + 0x20u;

int expect(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "XAPI time adapter: %s\n", message);
    }
    return condition ? 1 : 0;
}

int call_and_check(
    RecompFunction function,
    uint8_t *storage,
    uint32_t output,
    uint64_t *value)
{
    constexpr size_t storage_size = memory_size + 2u * guard_size;
    uint8_t before[storage_size];
    const uint32_t stack[] = {0xdeadbeefu, output};
    RecompRegisters expected = {
        0xa1a1a1a1u, 0xc2c2c2c2u, 0xd3d3d3d3u, 0xb4b4b4b4u,
        0x51515151u, 0x61616161u, 0x72727272u, entry_esp,
    };

    std::memset(storage, 0xa5, storage_size);
    std::memcpy(storage + guard_size + entry_esp - memory_base,
                stack, sizeof stack);
    std::memcpy(before, storage, sizeof before);
    recomp_runtime.registers = expected;
    function();
    expected.eax = 1u;
    expected.esp += 8u;

    int passed = expect(
        std::memcmp(&recomp_runtime.registers, &expected, sizeof expected) == 0,
        "EAX/ESP return or preserved registers differ");
    const size_t offset = guard_size + output - memory_base;
    passed &= expect(
        std::memcmp(storage, before, offset) == 0 &&
        std::memcmp(storage + offset + sizeof *value,
                    before + offset + sizeof *value,
                    storage_size - offset - sizeof *value) == 0,
        "write changed bytes outside the eight-byte output");
    std::memcpy(value, storage + offset, sizeof *value);
    return passed;
}

} // namespace

extern "C" int recomp_xapi_time_adapter_test(void)
{
    alignas(uint64_t) static uint8_t storage[memory_size + 2u * guard_size];
    const RecompMemoryRegion region = {
        memory_base, memory_size, storage + guard_size,
    };
    recomp_runtime_init(&region, 1u, nullptr, 0u, nullptr, 0u);

    const RecompFunction counter = recomp_xapi_time_lookup_manual(0x001830cfu);
    const RecompFunction frequency = recomp_xapi_time_lookup_manual(0x001830e0u);
    if (!expect(counter != nullptr && frequency != nullptr, "exact lookup failed")) {
        return 0;
    }
    int passed = expect(
        recomp_xapi_time_lookup_manual(0x001830ceu) == nullptr &&
        recomp_xapi_time_lookup_manual(0x001830d0u) == nullptr &&
        recomp_xapi_time_lookup_manual(0x001830dfu) == nullptr &&
        recomp_xapi_time_lookup_manual(0x001830e1u) == nullptr,
        "adjacent lookup resolved");
    passed &= expect(
        recomp_lookup_manual(0x001830cfu) == counter &&
        recomp_lookup_manual(0x001830e0u) == frequency,
        "manual lookup chain failed");

    uint64_t frequency_value = 0u;
    uint64_t edge_frequency = 0u;
    constexpr uint32_t output = memory_base + 0x80u;
    constexpr uint32_t edge_output = memory_base + memory_size - sizeof(uint64_t);
    passed &= call_and_check(frequency, storage, output, &frequency_value);
    passed &= call_and_check(frequency, storage, edge_output, &edge_frequency);
    passed &= expect(
        frequency_value == 1000000000ull &&
        edge_frequency == frequency_value &&
        recomp_xapi_performance_frequency() == frequency_value,
        "frequency is not the stable paired nanosecond frequency");
    if (frequency_value == 0u) {
        return 0;
    }

    using Clock = std::chrono::steady_clock;
    uint64_t first = 0u;
    uint64_t second = 0u;
    const auto first_before = Clock::now();
    const uint64_t model_before = recomp_xapi_performance_counter();
    passed &= call_and_check(counter, storage, output, &first);
    const auto first_after = Clock::now();

    // No guest presents or device updates occur between these counter queries.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const auto second_before = Clock::now();
    passed &= call_and_check(counter, storage, edge_output, &second);
    const uint64_t model_after = recomp_xapi_performance_counter();
    const auto second_after = Clock::now();
    passed &= expect(model_before <= first && first < second && second <= model_after,
                     "counter did not advance within its host model brackets");

    const double elapsed = static_cast<double>(second - first) /
                           static_cast<double>(frequency_value);
    const double minimum = std::chrono::duration<double>(second_before - first_after).count();
    const double maximum = std::chrono::duration<double>(second_after - first_before).count();
    // Bracket actual elapsed time rather than imposing a scheduler deadline.
    constexpr double tolerance = 0.001;
    passed &= expect(elapsed >= minimum - tolerance && elapsed <= maximum + tolerance,
                     "counter/frequency elapsed time disagrees with steady_clock");
    return passed;
}
