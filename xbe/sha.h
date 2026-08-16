#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace doaxbv {

std::string sha1Hex(const std::vector<std::uint8_t>& bytes);
std::string sha1Hex(const std::uint8_t* bytes, std::size_t size);
std::string sha256Hex(const std::vector<std::uint8_t>& bytes);

} // namespace doaxbv
