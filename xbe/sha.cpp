#include "sha.h"

#include <windows.h>
#include <array>
#include <bcrypt.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace doaxbv {
namespace {

std::string hexLower(const std::vector<std::uint8_t>& bytes)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto byte : bytes) {
        out << std::setw(2) << static_cast<int>(byte);
    }
    return out.str();
}

std::string hashHex(const wchar_t* algorithm, const std::uint8_t* bytes, std::size_t size)
{
    BCRYPT_ALG_HANDLE algorithmHandle = nullptr;
    BCRYPT_HASH_HANDLE hashHandle = nullptr;
    DWORD objectSize = 0;
    DWORD dataSize = 0;
    DWORD hashSize = 0;

    if (BCryptOpenAlgorithmProvider(&algorithmHandle, algorithm, nullptr, 0) != 0) {
        throw std::runtime_error("BCryptOpenAlgorithmProvider failed");
    }

    auto closeAlgorithm = [&]() {
        if (algorithmHandle != nullptr) {
            BCryptCloseAlgorithmProvider(algorithmHandle, 0);
        }
    };

    if (BCryptGetProperty(
            algorithmHandle,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectSize),
            sizeof(objectSize),
            &dataSize,
            0) != 0) {
        closeAlgorithm();
        throw std::runtime_error("BCryptGetProperty object length failed");
    }

    if (BCryptGetProperty(
            algorithmHandle,
            BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&hashSize),
            sizeof(hashSize),
            &dataSize,
            0) != 0) {
        closeAlgorithm();
        throw std::runtime_error("BCryptGetProperty hash length failed");
    }

    std::vector<std::uint8_t> objectBuffer(objectSize);
    std::vector<std::uint8_t> hash(hashSize);

    if (BCryptCreateHash(
            algorithmHandle,
            &hashHandle,
            objectBuffer.data(),
            objectSize,
            nullptr,
            0,
            0) != 0) {
        closeAlgorithm();
        throw std::runtime_error("BCryptCreateHash failed");
    }

    auto closeHash = [&]() {
        if (hashHandle != nullptr) {
            BCryptDestroyHash(hashHandle);
        }
        closeAlgorithm();
    };

    if (size > 0) {
        if (BCryptHashData(
                hashHandle,
                const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(bytes)),
                static_cast<ULONG>(size),
                0) != 0) {
            closeHash();
            throw std::runtime_error("BCryptHashData failed");
        }
    }

    if (BCryptFinishHash(hashHandle, hash.data(), static_cast<ULONG>(hash.size()), 0) != 0) {
        closeHash();
        throw std::runtime_error("BCryptFinishHash failed");
    }

    closeHash();
    return hexLower(hash);
}

} // namespace

std::string sha1Hex(const std::vector<std::uint8_t>& bytes)
{
    return sha1Hex(bytes.data(), bytes.size());
}

std::string sha1Hex(const std::uint8_t* bytes, std::size_t size)
{
    return hashHex(BCRYPT_SHA1_ALGORITHM, bytes, size);
}

std::string sha256Hex(const std::vector<std::uint8_t>& bytes)
{
    return hashHex(BCRYPT_SHA256_ALGORITHM, bytes.data(), bytes.size());
}

} // namespace doaxbv
