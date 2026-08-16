#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace doaxbv {

constexpr std::uint32_t kEntryRetailXor = 0xA8FC57AB;
constexpr std::uint32_t kEntryDebugXor = 0x94859D4B;
constexpr std::uint32_t kKernelRetailXor = 0x5B6D40B6;
constexpr std::uint32_t kKernelDebugXor = 0xEFB1F152;

struct XbeHeader {
    std::uint32_t baseAddress = 0;
    std::uint32_t sizeHeaders = 0;
    std::uint32_t sizeImage = 0;
    std::uint32_t sizeImageHeader = 0;
    std::uint32_t timeDate = 0;
    std::uint32_t certificateAddress = 0;
    std::uint32_t sectionCount = 0;
    std::uint32_t sectionHeadersAddress = 0;
    std::uint32_t initFlags = 0;
    std::uint32_t encodedEntryPoint = 0;
    std::uint32_t tlsAddress = 0;
    std::uint32_t stackCommit = 0;
    std::uint32_t heapReserve = 0;
    std::uint32_t heapCommit = 0;
    std::uint32_t peBaseAddress = 0;
    std::uint32_t peSizeImage = 0;
    std::uint32_t peChecksum = 0;
    std::uint32_t peTimeDate = 0;
    std::uint32_t pathnameAddress = 0;
    std::uint32_t filenameAddress = 0;
    std::uint32_t unicodeFilenameAddress = 0;
    std::uint32_t encodedKernelThunkAddress = 0;
    std::uint32_t nonKernelImportDirectoryAddress = 0;
    std::uint32_t libraryVersionCount = 0;
    std::uint32_t libraryVersionsAddress = 0;
    std::uint32_t kernelLibraryVersionAddress = 0;
    std::uint32_t xapiLibraryVersionAddress = 0;
    std::uint32_t logoBitmapAddress = 0;
    std::uint32_t logoBitmapSize = 0;
};

struct XbeCertificate {
    std::uint32_t size = 0;
    std::uint32_t timeDate = 0;
    std::uint32_t titleId = 0;
    std::string title;
    std::uint32_t mediaFlags = 0;
    std::uint32_t gameRegion = 0;
    std::uint32_t gameRatings = 0;
    std::uint32_t diskNumber = 0;
    std::uint32_t version = 0;
};

struct XbeSection {
    std::uint32_t flags = 0;
    std::uint32_t virtualAddress = 0;
    std::uint32_t virtualSize = 0;
    std::uint32_t rawAddress = 0;
    std::uint32_t rawSize = 0;
    std::uint32_t nameAddress = 0;
    std::string name;
    std::string expectedDigest;
    std::string computedDigest;
    bool digestMatches = false;
};

struct XbeTls {
    std::uint32_t dataStartAddress = 0;
    std::uint32_t dataEndAddress = 0;
    std::uint32_t tlsIndexAddress = 0;
    std::uint32_t callbackAddress = 0;
    std::uint32_t sizeOfZeroFill = 0;
    std::uint32_t characteristics = 0;
};

struct XbeLibraryVersion {
    std::string name;
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    std::uint16_t build = 0;
    std::uint16_t flags = 0;
};

struct XbeThunkEntry {
    std::uint32_t index = 0;
    std::uint32_t value = 0;
    std::uint32_t ordinal = 0;
};

struct XbeMetadata {
    std::uint64_t fileSize = 0;
    std::string fileSha1;
    std::string fileSha256;
    XbeHeader header;
    XbeCertificate certificate;
    std::uint32_t entryPointRetail = 0;
    std::uint32_t entryPointDebug = 0;
    std::uint32_t selectedEntryPoint = 0;
    std::string selectedEntryKind;
    std::uint32_t kernelThunkAddressRetail = 0;
    std::uint32_t kernelThunkAddressDebug = 0;
    std::uint32_t selectedKernelThunkAddress = 0;
    std::string selectedKernelThunkKind;
    std::vector<XbeSection> sections;
    std::optional<XbeTls> tls;
    std::vector<XbeLibraryVersion> libraries;
    std::vector<XbeThunkEntry> kernelThunks;
};

struct XbeParseResult {
    XbeMetadata metadata;
    std::vector<std::uint8_t> fileBytes;
    std::vector<std::string> errors;

    bool ok() const;
};

class XbeParser {
public:
    XbeParseResult parse(const std::filesystem::path& path) const;
};

std::string hex32(std::uint32_t value);
std::string hex64(std::uint64_t value);

} // namespace doaxbv
