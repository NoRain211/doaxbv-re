#include "xbe_parser.h"

#include "sha.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace doaxbv {
namespace {

constexpr std::uint32_t kHeaderSize = 0x178;
constexpr std::uint32_t kSectionHeaderSize = 0x38;
constexpr std::uint32_t kLibraryVersionSize = 0x10;
constexpr std::uint32_t kTlsSize = 0x18;

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
        | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::uint16_t readU16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

bool hasRange(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::size_t size)
{
    return offset <= bytes.size() && size <= bytes.size() - offset;
}

std::string bytesHex(const std::uint8_t* bytes, std::size_t size)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        out << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return out.str();
}

std::string sectionDigestHex(
    const std::vector<std::uint8_t>& bytes,
    std::uint32_t rawAddress,
    std::uint32_t rawSize)
{
    std::vector<std::uint8_t> digestInput;
    digestInput.reserve(static_cast<std::size_t>(rawSize) + 4);
    digestInput.push_back(static_cast<std::uint8_t>(rawSize & 0xFF));
    digestInput.push_back(static_cast<std::uint8_t>((rawSize >> 8) & 0xFF));
    digestInput.push_back(static_cast<std::uint8_t>((rawSize >> 16) & 0xFF));
    digestInput.push_back(static_cast<std::uint8_t>((rawSize >> 24) & 0xFF));
    digestInput.insert(
        digestInput.end(),
        bytes.begin() + rawAddress,
        bytes.begin() + rawAddress + rawSize);
    return sha1Hex(digestInput);
}

std::string readAsciiString(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    if (offset >= bytes.size()) {
        return {};
    }

    std::string value;
    for (std::size_t i = offset; i < bytes.size() && bytes[i] != 0; ++i) {
        const auto ch = bytes[i];
        if (ch < 0x20 || ch > 0x7e) {
            break;
        }
        value.push_back(static_cast<char>(ch));
    }
    return value;
}

std::string readFixedAscii(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::size_t size)
{
    std::string value;
    for (std::size_t i = 0; i < size && offset + i < bytes.size(); ++i) {
        const auto ch = bytes[offset + i];
        if (ch == 0) {
            break;
        }
        value.push_back(static_cast<char>(ch));
    }
    return value;
}

std::string readUtf16LeTitle(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::size_t byteSize)
{
    std::string value;
    for (std::size_t i = 0; i + 1 < byteSize; i += 2) {
        const auto code = static_cast<std::uint16_t>(bytes[offset + i])
            | static_cast<std::uint16_t>(bytes[offset + i + 1] << 8);
        if (code == 0) {
            break;
        }
        if (code >= 0x20 && code <= 0x7e) {
            value.push_back(static_cast<char>(code));
        } else {
            value.push_back('?');
        }
    }
    return value;
}

bool isInImageRange(const XbeHeader& header, std::uint32_t address)
{
    const auto start = static_cast<std::uint64_t>(header.baseAddress);
    const auto end = start + header.sizeImage;
    return address >= start && static_cast<std::uint64_t>(address) < end;
}

std::optional<std::size_t> vaToFileOffset(
    const XbeHeader& header,
    const std::vector<XbeSection>& sections,
    std::uint32_t address,
    std::size_t fileSize)
{
    if (address >= header.baseAddress) {
        const auto rva = static_cast<std::uint64_t>(address - header.baseAddress);
        if (rva < header.sizeHeaders && rva < fileSize) {
            return static_cast<std::size_t>(rva);
        }
    }

    for (const auto& section : sections) {
        const auto start = static_cast<std::uint64_t>(section.virtualAddress);
        const auto span = std::max(section.virtualSize, section.rawSize);
        const auto end = start + span;
        if (address >= start && static_cast<std::uint64_t>(address) < end) {
            const auto delta = static_cast<std::uint64_t>(address) - start;
            const auto fileOffset = static_cast<std::uint64_t>(section.rawAddress) + delta;
            if (fileOffset < fileSize) {
                return static_cast<std::size_t>(fileOffset);
            }
        }
    }

    return std::nullopt;
}

XbeHeader parseHeader(const std::vector<std::uint8_t>& bytes)
{
    XbeHeader header;
    header.baseAddress = readU32(bytes, 0x104);
    header.sizeHeaders = readU32(bytes, 0x108);
    header.sizeImage = readU32(bytes, 0x10C);
    header.sizeImageHeader = readU32(bytes, 0x110);
    header.timeDate = readU32(bytes, 0x114);
    header.certificateAddress = readU32(bytes, 0x118);
    header.sectionCount = readU32(bytes, 0x11C);
    header.sectionHeadersAddress = readU32(bytes, 0x120);
    header.initFlags = readU32(bytes, 0x124);
    header.encodedEntryPoint = readU32(bytes, 0x128);
    header.tlsAddress = readU32(bytes, 0x12C);
    header.stackCommit = readU32(bytes, 0x130);
    header.heapReserve = readU32(bytes, 0x134);
    header.heapCommit = readU32(bytes, 0x138);
    header.peBaseAddress = readU32(bytes, 0x13C);
    header.peSizeImage = readU32(bytes, 0x140);
    header.peChecksum = readU32(bytes, 0x144);
    header.peTimeDate = readU32(bytes, 0x148);
    header.pathnameAddress = readU32(bytes, 0x14C);
    header.filenameAddress = readU32(bytes, 0x150);
    header.unicodeFilenameAddress = readU32(bytes, 0x154);
    header.encodedKernelThunkAddress = readU32(bytes, 0x158);
    header.nonKernelImportDirectoryAddress = readU32(bytes, 0x15C);
    header.libraryVersionCount = readU32(bytes, 0x160);
    header.libraryVersionsAddress = readU32(bytes, 0x164);
    header.kernelLibraryVersionAddress = readU32(bytes, 0x168);
    header.xapiLibraryVersionAddress = readU32(bytes, 0x16C);
    header.logoBitmapAddress = readU32(bytes, 0x170);
    header.logoBitmapSize = readU32(bytes, 0x174);
    return header;
}

} // namespace

bool XbeParseResult::ok() const
{
    return errors.empty();
}

XbeParseResult XbeParser::parse(const std::filesystem::path& path) const
{
    XbeParseResult result;

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        result.errors.push_back("failed to open XBE");
        return result;
    }

    std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    result.fileBytes = bytes;
    result.metadata.fileSize = bytes.size();

    if (bytes.size() < kHeaderSize) {
        result.errors.push_back("file is smaller than XBE image header");
        return result;
    }

    result.metadata.fileSha1 = sha1Hex(bytes);
    result.metadata.fileSha256 = sha256Hex(bytes);

    if (bytes[0] != 'X' || bytes[1] != 'B' || bytes[2] != 'E' || bytes[3] != 'H') {
        result.errors.push_back("XBE magic mismatch");
        return result;
    }

    auto& metadata = result.metadata;
    metadata.header = parseHeader(bytes);
    metadata.entryPointRetail = metadata.header.encodedEntryPoint ^ kEntryRetailXor;
    metadata.entryPointDebug = metadata.header.encodedEntryPoint ^ kEntryDebugXor;
    metadata.kernelThunkAddressRetail = metadata.header.encodedKernelThunkAddress ^ kKernelRetailXor;
    metadata.kernelThunkAddressDebug = metadata.header.encodedKernelThunkAddress ^ kKernelDebugXor;

    if (isInImageRange(metadata.header, metadata.entryPointRetail)) {
        metadata.selectedEntryPoint = metadata.entryPointRetail;
        metadata.selectedEntryKind = "retail";
    } else if (isInImageRange(metadata.header, metadata.entryPointDebug)) {
        metadata.selectedEntryPoint = metadata.entryPointDebug;
        metadata.selectedEntryKind = "debug";
    } else {
        result.errors.push_back("neither decoded entry point is inside the XBE image range");
    }

    const auto sectionTableOffset = vaToFileOffset(
        metadata.header,
        metadata.sections,
        metadata.header.sectionHeadersAddress,
        bytes.size());
    if (!sectionTableOffset.has_value()) {
        result.errors.push_back("section table address does not map to the file");
        return result;
    }

    for (std::uint32_t i = 0; i < metadata.header.sectionCount; ++i) {
        const auto offset = *sectionTableOffset + static_cast<std::size_t>(i) * kSectionHeaderSize;
        if (!hasRange(bytes, offset, kSectionHeaderSize)) {
            result.errors.push_back("section header extends past file");
            break;
        }

        XbeSection section;
        section.flags = readU32(bytes, offset + 0x00);
        section.virtualAddress = readU32(bytes, offset + 0x04);
        section.virtualSize = readU32(bytes, offset + 0x08);
        section.rawAddress = readU32(bytes, offset + 0x0C);
        section.rawSize = readU32(bytes, offset + 0x10);
        section.nameAddress = readU32(bytes, offset + 0x14);
        section.expectedDigest = bytesHex(bytes.data() + offset + 0x24, 0x14);

        if (!hasRange(bytes, section.rawAddress, section.rawSize)) {
            result.errors.push_back("section raw data extends past file");
        } else {
            section.computedDigest = sectionDigestHex(bytes, section.rawAddress, section.rawSize);
            section.digestMatches = section.computedDigest == section.expectedDigest;
        }

        metadata.sections.push_back(section);
    }

    for (auto& section : metadata.sections) {
        const auto nameOffset = vaToFileOffset(
            metadata.header,
            metadata.sections,
            section.nameAddress,
            bytes.size());
        if (nameOffset.has_value()) {
            section.name = readAsciiString(bytes, *nameOffset);
        }
    }

    const auto certificateOffset = vaToFileOffset(
        metadata.header,
        metadata.sections,
        metadata.header.certificateAddress,
        bytes.size());
    if (!certificateOffset.has_value() || !hasRange(bytes, *certificateOffset, 0xC8)) {
        result.errors.push_back("certificate address does not map to a complete certificate header");
    } else {
        auto& certificate = metadata.certificate;
        certificate.size = readU32(bytes, *certificateOffset + 0x00);
        certificate.timeDate = readU32(bytes, *certificateOffset + 0x04);
        certificate.titleId = readU32(bytes, *certificateOffset + 0x08);
        certificate.title = readUtf16LeTitle(bytes, *certificateOffset + 0x0C, 0x50);
        certificate.mediaFlags = readU32(bytes, *certificateOffset + 0xB4);
        certificate.gameRegion = readU32(bytes, *certificateOffset + 0xB8);
        certificate.gameRatings = readU32(bytes, *certificateOffset + 0xBC);
        certificate.diskNumber = readU32(bytes, *certificateOffset + 0xC0);
        certificate.version = readU32(bytes, *certificateOffset + 0xC4);
    }

    if (metadata.header.tlsAddress != 0) {
        const auto tlsOffset = vaToFileOffset(
            metadata.header,
            metadata.sections,
            metadata.header.tlsAddress,
            bytes.size());
        if (!tlsOffset.has_value() || !hasRange(bytes, *tlsOffset, kTlsSize)) {
            result.errors.push_back("TLS address does not map to a complete TLS table");
        } else {
            XbeTls tls;
            tls.dataStartAddress = readU32(bytes, *tlsOffset + 0x00);
            tls.dataEndAddress = readU32(bytes, *tlsOffset + 0x04);
            tls.tlsIndexAddress = readU32(bytes, *tlsOffset + 0x08);
            tls.callbackAddress = readU32(bytes, *tlsOffset + 0x0C);
            tls.sizeOfZeroFill = readU32(bytes, *tlsOffset + 0x10);
            tls.characteristics = readU32(bytes, *tlsOffset + 0x14);
            metadata.tls = tls;
        }
    }

    const auto libraryOffset = vaToFileOffset(
        metadata.header,
        metadata.sections,
        metadata.header.libraryVersionsAddress,
        bytes.size());
    if (metadata.header.libraryVersionCount > 0) {
        if (!libraryOffset.has_value()) {
            result.errors.push_back("library version table address does not map to the file");
        } else {
            for (std::uint32_t i = 0; i < metadata.header.libraryVersionCount; ++i) {
                const auto offset = *libraryOffset + static_cast<std::size_t>(i) * kLibraryVersionSize;
                if (!hasRange(bytes, offset, kLibraryVersionSize)) {
                    result.errors.push_back("library version table extends past file");
                    break;
                }

                XbeLibraryVersion library;
                library.name = readFixedAscii(bytes, offset, 8);
                library.major = readU16(bytes, offset + 0x08);
                library.minor = readU16(bytes, offset + 0x0A);
                library.build = readU16(bytes, offset + 0x0C);
                library.flags = readU16(bytes, offset + 0x0E);
                metadata.libraries.push_back(library);
            }
        }
    }

    if (isInImageRange(metadata.header, metadata.kernelThunkAddressRetail)) {
        metadata.selectedKernelThunkAddress = metadata.kernelThunkAddressRetail;
        metadata.selectedKernelThunkKind = "retail";
    } else if (isInImageRange(metadata.header, metadata.kernelThunkAddressDebug)) {
        metadata.selectedKernelThunkAddress = metadata.kernelThunkAddressDebug;
        metadata.selectedKernelThunkKind = "debug";
    } else {
        result.errors.push_back("neither decoded kernel thunk address is inside the XBE image range");
    }

    if (metadata.selectedKernelThunkAddress != 0) {
        const auto thunkOffset = vaToFileOffset(
            metadata.header,
            metadata.sections,
            metadata.selectedKernelThunkAddress,
            bytes.size());
        if (!thunkOffset.has_value()) {
            result.errors.push_back("kernel thunk table does not map to the file");
        } else {
            for (std::uint32_t index = 0; index < 4096; ++index) {
                const auto offset = *thunkOffset + static_cast<std::size_t>(index) * 4;
                if (!hasRange(bytes, offset, 4)) {
                    result.errors.push_back("kernel thunk table extends past file before terminator");
                    break;
                }

                const auto value = readU32(bytes, offset);
                if (value == 0) {
                    break;
                }

                XbeThunkEntry entry;
                entry.index = index;
                entry.value = value;
                entry.ordinal = value & 0x1FF;
                metadata.kernelThunks.push_back(entry);
            }
        }
    }

    return result;
}

std::string hex32(std::uint32_t value)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::nouppercase << std::setw(8)
        << std::setfill('0') << value;
    return out.str();
}

std::string hex64(std::uint64_t value)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::nouppercase << std::setw(16)
        << std::setfill('0') << value;
    return out.str();
}

} // namespace doaxbv
