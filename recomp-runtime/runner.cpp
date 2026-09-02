#include "host_diagnostics.h"
#ifdef RECOMP_FULL_PROGRAM
#include "cri_service_adapter.h"
#include "d3d_presenter.h"
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
    bool vsyncPresent = false;
    std::vector<std::uint64_t> inputStartPulsePolls;
    std::vector<std::uint64_t> inputAPulsePolls;
std::vector<std::pair<std::uint64_t, std::uint16_t>> inputButtonsPulses;
struct AnalogPulse {
    std::uint64_t poll;
    std::uint8_t index;
    std::uint8_t value;
};
std::vector<AnalogPulse> inputAnalogPulses;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool wantsValue = arg == "--xbe" || arg == "--expect-stop" ||
            arg == "--stop-at" || arg == "--milestone-log" ||
            arg == "--input-start-pulse-at" ||
            arg == "--input-a-pulse-at" || arg == "--input-buttons-at" ||
            arg == "--input-analog-at";

        if (wantsValue && i + 1 >= argc) {
            std::cerr << "recomp runner: " << arg << " needs a value\n";
            return 64;
        }
        if (arg == "--xbe") {
            xbePath = argv[++i];
        } else if (arg == "--vsync") {
            vsyncPresent = true;
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
        } else if (arg == "--input-a-pulse-at") {
            const std::string value = argv[++i];
            std::uint64_t parsedPoll = 0u;
            if (!parsePositiveU64(value, parsedPoll)) {
                std::cerr << "recomp runner: invalid A pulse poll '"
                          << value << "'\n";
                return 64;
            }
            if (inputAPulsePolls.size() >=
                    RECOMP_INPUT_A_PULSE_POLL_CAPACITY) {
                std::cerr << "recomp runner: at most "
                          << RECOMP_INPUT_A_PULSE_POLL_CAPACITY
                          << " A pulse polls\n";
                return 64;
            }
            inputAPulsePolls.push_back(parsedPoll);
            inputStartPulseEnabled = true;
        } else if (arg == "--input-buttons-at") {
            const std::string value = argv[++i];
            const std::size_t separator = value.find(':');
            if (separator == std::string::npos) {
                std::cerr << "recomp runner: invalid button pulse '"
                          << value << "' (expected <poll>:<hexmask>)\n";
                return 64;
            }
            std::uint64_t parsedPoll = 0u;
            if (!parsePositiveU64(value.substr(0u, separator), parsedPoll)) {
                std::cerr << "recomp runner: invalid button pulse poll '"
                          << value.substr(0u, separator) << "'\n";
                return 64;
            }
            std::uint16_t parsedMask = 0u;
            try {
                const unsigned long mask =
                    std::stoul(value.substr(separator + 1u), nullptr, 16);
                if (mask == 0u || mask > 0xfffful) {
                    std::cerr << "recomp runner: invalid button pulse mask '"
                              << value.substr(separator + 1u) << "'\n";
                    return 64;
                }
                /* The Xbox digital word has eight bits, 0x0001-0x0080. The
                   face buttons, Black/White and the triggers are analog and
                   travel in a separate byte array, so a high bit here is set,
                   copied to the guest, and correctly ignored -- silently. Six
                   rounds of mask sweeps were partly measuring nothing before
                   this was caught. Fail closed instead. */
                if ((mask & ~0x00fful) != 0u) {
                    std::cerr << "recomp runner: button mask '"
                              << value.substr(separator + 1u)
                              << "' has bits above 0x00ff, which the digital"
                                 " button word cannot carry.\n"
                              << "  legal: 0x0001-0x0008 dpad, 0x0010 START,"
                                 " 0x0020 BACK, 0x0040/0x0080 thumb clicks\n"
                              << "  for A/B/X/Y, Black/White and the triggers"
                                 " use --input-analog-at <poll>:<index>:<value>"
                                 "\n";
                    return 64;
                }
                parsedMask = static_cast<std::uint16_t>(mask);
            } catch (const std::exception&) {
                std::cerr << "recomp runner: invalid button pulse mask '"
                          << value.substr(separator + 1u) << "'\n";
                return 64;
            }
            if (inputButtonsPulses.size() >=
                    RECOMP_INPUT_BUTTONS_PULSE_CAPACITY) {
                std::cerr << "recomp runner: at most "
                          << RECOMP_INPUT_BUTTONS_PULSE_CAPACITY
                          << " button pulses\n";
                return 64;
            }
            inputButtonsPulses.emplace_back(parsedPoll, parsedMask);
            inputStartPulseEnabled = true;
        } else if (arg == "--input-analog-at") {
            /* <poll>:<index>:<value>. Index is the guest analog ordering from
               xinput_xbox.h: 0=A 1=B 2=X 3=Y 4=Black 5=White 6=LTrig 7=RTrig.
               Value is 0-255; the guest threshold is 30. */
            const std::string value = argv[++i];
            const std::size_t first = value.find(':');
            const std::size_t second =
                first == std::string::npos ? std::string::npos
                                           : value.find(':', first + 1u);
            if (first == std::string::npos || second == std::string::npos) {
                std::cerr << "recomp runner: invalid analog pulse '" << value
                          << "' (expected <poll>:<index>:<value>)\n";
                return 64;
            }
            std::uint64_t parsedPoll = 0u;
            if (!parsePositiveU64(value.substr(0u, first), parsedPoll)) {
                std::cerr << "recomp runner: invalid analog pulse poll '"
                          << value.substr(0u, first) << "'\n";
                return 64;
            }
            std::uint8_t parsedIndex = 0u;
            std::uint8_t parsedValue = 0u;
            try {
                const unsigned long index = std::stoul(
                    value.substr(first + 1u, second - first - 1u), nullptr, 10);
                const unsigned long level =
                    std::stoul(value.substr(second + 1u), nullptr, 10);
                if (index >= RECOMP_INPUT_ANALOG_BUTTON_COUNT) {
                    std::cerr << "recomp runner: analog index " << index
                              << " out of range, expected 0-7"
                                 " (0=A 1=B 2=X 3=Y 4=Black 5=White"
                                 " 6=LTrig 7=RTrig)\n";
                    return 64;
                }
                if (level > 0xfful) {
                    std::cerr << "recomp runner: analog value " << level
                              << " out of range, expected 0-255\n";
                    return 64;
                }
                parsedIndex = static_cast<std::uint8_t>(index);
                parsedValue = static_cast<std::uint8_t>(level);
            } catch (const std::exception&) {
                std::cerr << "recomp runner: invalid analog pulse '" << value
                          << "'\n";
                return 64;
            }
            if (inputAnalogPulses.size() >=
                    RECOMP_INPUT_ANALOG_PULSE_CAPACITY) {
                std::cerr << "recomp runner: at most "
                          << RECOMP_INPUT_ANALOG_PULSE_CAPACITY
                          << " analog pulses\n";
                return 64;
            }
            inputAnalogPulses.push_back(
                AnalogPulse{parsedPoll, parsedIndex, parsedValue});
            inputStartPulseEnabled = true;
        } else {
            std::cerr << "usage: recomp_runner --xbe <path>"
                         " [--expect-stop <id>] [--stop-at <id>]"
                         " [--milestone-log <path>]"
                         " [--vsync]"
                         " [--input-start-pulse-at <poll>]"
                         " [--input-buttons-at <poll>:<hexmask>]"
                         " [--input-analog-at <poll>:<index>:<value>]\n";
            return 64;
        }
    }
    if (xbePath.empty()) {
        std::cerr << "usage: recomp_runner --xbe <path>"
                     " [--expect-stop <id>] [--stop-at <id>]"
                     " [--milestone-log <path>]"
                     " [--input-start-pulse-at <poll>]"
                     " [--input-buttons-at <poll>:<hexmask>]"
                     " [--input-analog-at <poll>:<index>:<value>]\n";
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
    /* The guest frame loop does not depend on wall-clock time (kernel waits
       are immediate), so DXGI vsync is unintended host pacing. Run presents
       unthrottled so natural frame-driven transitions are reachable inside
       the gate budget. This removes pacing; it does not force guest state.

       Unthrottled presenting is correct for a gate and wrong for watching.
       The guest presents as fast as the host will retire frames -- measured
       at 255 presents/sec -- into a windowed DXGI_SWAP_EFFECT_DISCARD chain
       on a 60 Hz output. Four or more presents land inside one scanout, so
       the display shows bands from different frames and the image appears to
       roll vertically. --vsync paces Present() to the output refresh for a
       human observer; it changes host pacing only, never guest state. */
    recomp_d3d_presenter_set_immediate_present(!vsyncPresent);
    if (inputStartPulseEnabled) {
        recomp_input_pulse_source_init(
            &inputPulseSource,
            nullptr,
            inputStartPulsePolls.empty() ? 0u : inputStartPulsePolls.front());
        for (std::size_t i = 1u; i < inputStartPulsePolls.size(); ++i) {
            recomp_input_pulse_source_add_poll(
                &inputPulseSource, inputStartPulsePolls[i]);
        }
        for (std::uint64_t poll : inputAPulsePolls) {
            recomp_input_pulse_source_add_a_poll(&inputPulseSource, poll);
        }
        for (const auto& buttonsPulse : inputButtonsPulses) {
            recomp_input_pulse_source_add_buttons_poll(
                &inputPulseSource, buttonsPulse.first, buttonsPulse.second);
        }
        for (const auto& analogPulse : inputAnalogPulses) {
            recomp_input_pulse_source_add_analog_poll(
                &inputPulseSource, analogPulse.poll, analogPulse.index,
                analogPulse.value);
        }
        recomp_input_adapter_set_source(sampleInputPulse);
        std::cerr << "recomp input: deterministic START pulse poll=";
        for (std::size_t i = 0u; i < inputStartPulsePolls.size(); ++i) {
            std::cerr << (i == 0u ? "" : ",") << inputStartPulsePolls[i];
        }
        std::cerr << " A pulse poll=";
        for (std::size_t i = 0u; i < inputAPulsePolls.size(); ++i) {
            std::cerr << (i == 0u ? "" : ",") << inputAPulsePolls[i];
        }
        std::cerr << " button pulses=";
        for (std::size_t i = 0u; i < inputButtonsPulses.size(); ++i) {
            std::cerr << (i == 0u ? "" : ",") << std::hex
                      << inputButtonsPulses[i].first << ":"
                      << inputButtonsPulses[i].second;
        }
        std::cerr << std::dec;
        std::cerr << " analog pulses=";
        for (std::size_t i = 0u; i < inputAnalogPulses.size(); ++i) {
            std::cerr << (i == 0u ? "" : ",") << inputAnalogPulses[i].poll
                      << ":" << static_cast<unsigned>(inputAnalogPulses[i].index)
                      << ":"
                      << static_cast<unsigned>(inputAnalogPulses[i].value);
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
