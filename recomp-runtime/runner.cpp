#include "host_diagnostics.h"
#ifdef RECOMP_FULL_PROGRAM
#include "cri_service_adapter.h"
#include "fiber_adapter.h"
#include "input_adapter.h"
#include "input_host_win32.h"
#include "input_pulse_source.h"
#endif
#include "runtime.h"
#include "stop_report.h"
#include "xbox_memory_layout.h"
#include "xbe_parser.h"

#ifndef RECOMP_FULL_PROGRAM
#include "runner_imports.h"
#endif

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

extern "C" const char *recomp_disc_root_path;

constexpr std::size_t kRamSize = 64u * 1024u * 1024u;
constexpr std::uint32_t kStackTop = 0x00f7fff0u;
constexpr std::uint32_t kKernelDataBase = 0x00740000u;

extern "C" void xbe_entry_point(void);
#ifdef RECOMP_FULL_PROGRAM
extern "C" RecompFunction recomp_program_lookup(
    std::uint32_t guest_address);

RecompInputPulseSource inputPulseSource;

bool sampleInputPulse(RecompInputGamepad *gamepad)
{
    return recomp_input_pulse_source_sample(&inputPulseSource, gamepad);
}
#endif

bool parsePositiveU64(const std::string& value, std::uint64_t& parsed)
{
    if (value.empty()) {
        return false;
    }

    std::uint64_t result = 0u;
    for (char character : value) {
        if (character < '0' || character > '9') {
            return false;
        }
        const std::uint64_t digit =
            static_cast<std::uint64_t>(character - '0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) /
                10u) {
            return false;
        }
        result = result * 10u + digit;
    }
    if (result == 0u) {
        return false;
    }

    parsed = result;
    return true;
}

void writeU16(
    std::vector<std::uint8_t>& memory,
    std::uint32_t address,
    std::uint16_t value)
{
    memory[address] = static_cast<std::uint8_t>(value);
    memory[address + 1u] = static_cast<std::uint8_t>(value >> 8u);
}

void writeU32(
    std::vector<std::uint8_t>& memory,
    std::uint32_t address,
    std::uint32_t value)
{
    writeU16(memory, address, static_cast<std::uint16_t>(value));
    writeU16(memory, address + 2u, static_cast<std::uint16_t>(value >> 16u));
}

/* The eleven data exports this XBE imports. Every other imported ordinal is a
   function and must stay callable so an unimplemented one fails by name. */
std::uint32_t kernelDataAddress(std::uint32_t ordinal)
{
    switch (ordinal) {
    case 322u: return kKernelDataBase;          // XboxHardwareInfo
    case 324u: return kKernelDataBase + 0x10u;  // XboxKrnlVersion
    case 16u: return kKernelDataBase + 0x20u;   // ExEventObjectType
    case 40u: return kKernelDataBase + 0x30u;   // HalDiskCachePartitionCount
    case 156u: return kKernelDataBase + 0x40u;  // KeTickCount
    case 164u: return kKernelDataBase + 0x50u;  // LaunchDataPage
    case 259u: return kKernelDataBase + 0x60u;  // PsThreadObjectType
    case 323u: return kKernelDataBase + 0x70u;  // XboxHDKey
    case 325u: return kKernelDataBase + 0x80u;  // XboxSignatureKey
    case 354u: return kKernelDataBase + 0x90u;  // XboxAlternateSignatureKeys
    case 356u: return kKernelDataBase + 0xa0u;  // HalBootSMCVideoMode
    default: return 0u;
    }
}

bool materializeKernelDataExports(
    const doaxbv::XbeMetadata& metadata,
    std::vector<std::uint8_t>& memory,
    std::string& error)
{
    if (kKernelDataBase + 0x420u > memory.size()) {
        error = "kernel data area leaves guest RAM";
        return false;
    }

    writeU32(memory, kKernelDataBase, 0u);
    memory[kKernelDataBase + 4u] = 0xa1u;
    memory[kKernelDataBase + 5u] = 0xb1u;
    writeU16(memory, kKernelDataBase + 0x10u, 1u);
    writeU16(memory, kKernelDataBase + 0x12u, 0u);
    writeU16(memory, kKernelDataBase + 0x14u, 5849u);
    writeU16(memory, kKernelDataBase + 0x16u, 0u);
    writeU32(memory, kKernelDataBase + 0x30u, 3u);

    for (const auto& thunk : metadata.kernelThunks) {
        const std::uint32_t dataAddress = kernelDataAddress(thunk.ordinal);

        if (dataAddress == 0u) {
            continue;
        }
        const std::uint64_t slot =
            static_cast<std::uint64_t>(metadata.selectedKernelThunkAddress) +
            static_cast<std::uint64_t>(thunk.index) * 4u;
        if (slot + 4u > memory.size()) {
            error = "kernel data thunk slot leaves guest RAM";
            return false;
        }
        writeU32(memory, static_cast<std::uint32_t>(slot), dataAddress);
    }
    return true;
}

bool materializeStartupThread(
    const doaxbv::XbeMetadata& metadata,
    std::vector<std::uint8_t>& memory,
    std::string& error)
{
    if (!metadata.tls.has_value()) {
        return true;
    }

    const auto& tls = *metadata.tls;
    if (tls.dataEndAddress < tls.dataStartAddress) {
        error = "TLS initialized-data range is reversed";
        return false;
    }
    const std::uint64_t tlsSize =
        static_cast<std::uint64_t>(tls.dataEndAddress) -
        tls.dataStartAddress + tls.sizeOfZeroFill;
    const std::uint64_t tlsBlock =
        static_cast<std::uint64_t>(XBOX_STARTUP_THREAD_STACK_SLOT) + 4u;
    const std::uint64_t stackBase = tlsBlock + tlsSize;
    if (stackBase > memory.size() ||
        static_cast<std::uint64_t>(XBOX_STARTUP_THREAD_OBJECT) + 0x2cu >
            memory.size()) {
        error = "startup thread state leaves guest RAM";
        return false;
    }

    std::memset(
        memory.data() + XBOX_STARTUP_THREAD_STACK_SLOT,
        0,
        static_cast<std::size_t>(4u + tlsSize));
    std::memset(memory.data() + XBOX_STARTUP_THREAD_OBJECT, 0, 0x2cu);

    // The lifter flattens FS-relative PCR reads to guest address zero.
    writeU32(memory, 0x00u, 0xffffffffu); // ExceptionList
    writeU32(memory, 0x04u, static_cast<std::uint32_t>(stackBase));
    writeU32(memory, 0x1cu, 0u);          // SelfPcr
    writeU32(memory, 0x20u, 0x28u);      // Prcb
    writeU32(memory, 0x24u, 0u);         // Irql
    writeU32(memory, 0x28u, XBOX_STARTUP_THREAD_OBJECT);
    writeU32(
        memory,
        XBOX_STARTUP_THREAD_OBJECT + 0x28u,
        XBOX_STARTUP_THREAD_STACK_SLOT);
    return true;
}

bool mapXbe(
    const doaxbv::XbeParseResult& parsed,
    std::vector<std::uint8_t>& memory,
    std::string& error)
{
    const auto& metadata = parsed.metadata;
    const std::uint64_t image_end =
        static_cast<std::uint64_t>(metadata.header.baseAddress) +
        metadata.header.sizeImage;
    const std::uint64_t header_end =
        static_cast<std::uint64_t>(metadata.header.baseAddress) +
        metadata.header.sizeHeaders;

    if (image_end > memory.size() || header_end > memory.size() ||
        metadata.header.sizeHeaders > parsed.fileBytes.size()) {
        error = "XBE image or headers leave guest RAM";
        return false;
    }

    std::memcpy(
        memory.data() + metadata.header.baseAddress,
        parsed.fileBytes.data(),
        metadata.header.sizeHeaders);

    for (const auto& section : metadata.sections) {
        const std::uint64_t virtual_end =
            static_cast<std::uint64_t>(section.virtualAddress) +
            std::max(section.virtualSize, section.rawSize);
        const std::uint64_t raw_end =
            static_cast<std::uint64_t>(section.rawAddress) + section.rawSize;

        if (virtual_end > memory.size() || raw_end > parsed.fileBytes.size()) {
            error = "XBE section leaves its source or guest-RAM range";
            return false;
        }
        if (!section.digestMatches) {
            error = "XBE section digest mismatch: " + section.name;
            return false;
        }

        std::memset(
            memory.data() + section.virtualAddress,
            0,
            std::max(section.virtualSize, section.rawSize));
        std::memcpy(
            memory.data() + section.virtualAddress,
            parsed.fileBytes.data() + section.rawAddress,
            section.rawSize);
    }

    if (!materializeKernelDataExports(metadata, memory, error)) {
        return false;
    }
    if (!materializeStartupThread(metadata, memory, error)) {
        return false;
    }

    return true;
}

#ifndef RECOMP_FULL_PROGRAM
const char *importName(RecompImportKind kind)
{
    switch (kind) {
    case RECOMP_IMPORT_CREATE_THREAD:
        return "CreateThread";
    case RECOMP_IMPORT_BOOT_TO_DASH:
        return "XapiBootToDash";
    case RECOMP_IMPORT_EXIT_THREAD:
        return "ExitThread";
    default:
        return "none";
    }
}
#endif

} // namespace

int main(int argc, char **argv)
{
    recomp_install_host_diagnostics();

    std::string xbePath;
    std::string expectStop;
    std::string stopAt;
    std::string milestoneLog;
    bool inputStartPulseEnabled = false;
    std::vector<std::uint64_t> inputStartPulsePolls;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool wantsValue = arg == "--xbe" || arg == "--expect-stop" ||
            arg == "--stop-at" || arg == "--milestone-log" ||
            arg == "--input-start-pulse-at";

        if (wantsValue && i + 1 >= argc) {
            std::cerr << "recomp runner: " << arg << " needs a value\n";
            return 64;
        }
        if (arg == "--xbe") {
            xbePath = argv[++i];
        } else if (arg == "--expect-stop") {
            expectStop = argv[++i];
        } else if (arg == "--stop-at") {
            stopAt = argv[++i];
        } else if (arg == "--milestone-log") {
            milestoneLog = argv[++i];
        } else if (arg == "--input-start-pulse-at") {
            const std::string value = argv[++i];
            std::uint64_t parsedPoll = 0u;
            if (!parsePositiveU64(value, parsedPoll)) {
                std::cerr << "recomp runner: invalid START pulse poll '"
                          << value << "'\n";
                return 64;
            }
            if (inputStartPulsePolls.size() >=
                    RECOMP_INPUT_PULSE_POLL_CAPACITY) {
                std::cerr << "recomp runner: at most "
                          << RECOMP_INPUT_PULSE_POLL_CAPACITY
                          << " START pulse polls\n";
                return 64;
            }
            inputStartPulsePolls.push_back(parsedPoll);
            inputStartPulseEnabled = true;
        } else {
            std::cerr << "usage: recomp_runner --xbe <path>"
                         " [--expect-stop <id>] [--stop-at <id>]"
                         " [--milestone-log <path>]"
                         " [--input-start-pulse-at <poll>]\n";
            return 64;
        }
    }
    if (xbePath.empty()) {
        std::cerr << "usage: recomp_runner --xbe <path>"
                     " [--expect-stop <id>] [--stop-at <id>]"
                     " [--milestone-log <path>]"
                     " [--input-start-pulse-at <poll>]\n";
        return 64;
    }
    recomp_stop_configure(
        expectStop.empty() ? nullptr : expectStop.c_str(),
        milestoneLog.empty() ? nullptr : milestoneLog.c_str());
    recomp_stop_configure_boundary(
        stopAt.empty() ? nullptr : stopAt.c_str());

    doaxbv::XbeParser parser;
    doaxbv::XbeParseResult parsed = parser.parse(xbePath.c_str());
    if (!parsed.ok()) {
        std::cerr << "recomp runner: " << parsed.errors.front() << '\n';
        return 1;
    }

    {
        std::filesystem::path xbe_path = std::filesystem::absolute(xbePath);
        std::filesystem::path parent = xbe_path.parent_path();
        if (parent.empty()) {
            parent = std::filesystem::current_path();
        }
        static std::string disc_root = parent.string();
        recomp_disc_root_path = disc_root.c_str();
    }

    std::vector<std::uint8_t> memory(kRamSize, 0u);
    std::string error;
    if (!mapXbe(parsed, memory, error)) {
        std::cerr << "recomp runner: " << error << '\n';
        return 1;
    }

    const RecompMemoryRegion region = {
        0u,
        memory.size(),
        memory.data(),
    };
#ifndef RECOMP_FULL_PROGRAM
    std::vector<RecompMemoryAccess> accesses(4096u);
    const RecompFunctionEntry functions[] = {
        {parsed.metadata.selectedEntryPoint, xbe_entry_point},
    };
#endif

    recomp_runtime_init(
        &region,
        1u,
#ifdef RECOMP_FULL_PROGRAM
        nullptr,
        0u,
        nullptr,
        0u);
    recomp_runtime_set_lookup(recomp_program_lookup);
#else
        accesses.data(),
        accesses.size(),
        functions,
        1u);
#endif
    recomp_runtime.registers.esp = kStackTop;
#ifdef RECOMP_FULL_PROGRAM
    recomp_cri_service_adapter_reset();
    recomp_fiber_adapter_reset();
    recomp_input_adapter_reset();
    if (inputStartPulseEnabled) {
        recomp_input_pulse_source_init(
            &inputPulseSource,
            nullptr,
            inputStartPulsePolls.front());
        for (std::size_t i = 1u; i < inputStartPulsePolls.size(); ++i) {
            recomp_input_pulse_source_add_poll(
                &inputPulseSource, inputStartPulsePolls[i]);
        }
        recomp_input_adapter_set_source(sampleInputPulse);
        std::cerr << "recomp input: deterministic START pulse poll=";
        for (std::size_t i = 0u; i < inputStartPulsePolls.size(); ++i) {
            std::cerr << (i == 0u ? "" : ",") << inputStartPulsePolls[i];
        }
        std::cerr << '\n';
    } else {
        recomp_input_adapter_set_source(recomp_input_host_sample);
    }
#endif
#ifndef RECOMP_FULL_PROGRAM
    recomp_imports_reset();
#endif

    if (!recomp_dispatch(parsed.metadata.selectedEntryPoint)) {
        std::cerr << "recomp runner: entry point is not registered\n";
        return 2;
    }

#ifdef RECOMP_FULL_PROGRAM
    std::cout << "entry=" << doaxbv::hex32(parsed.metadata.selectedEntryPoint)
              << " sha256=" << parsed.metadata.fileSha256 << '\n';
#else
    const RecompImportState *imports = recomp_imports_state();
    std::cout << "entry=" << doaxbv::hex32(parsed.metadata.selectedEntryPoint)
              << " sha256=" << parsed.metadata.fileSha256
              << " first_import=" << importName(imports->first_import)
              << " thread_start=" << doaxbv::hex32(imports->thread_start)
              << '\n';

    if (imports->first_import == RECOMP_IMPORT_CREATE_THREAD &&
        imports->thread_start != 0u) {
        std::cerr << "recomp runner: stopped at unimplemented thread start "
                  << doaxbv::hex32(imports->thread_start) << '\n';
        return 2;
    }
#endif

    /* Reaching here means the entry point returned rather than stopping at a
       named boundary. Route it through the same reporter so an expectation of
       "completed" can be asserted and the milestone log stays complete. */
    recomp_stop(0, "completed");
    return 0;
}
