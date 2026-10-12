#pragma once
#include <array>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace luma {
inline std::string newID() {
    std::random_device random;
    std::array<unsigned char, 16> bytes{};
    for (auto &byte : bytes)
        byte = static_cast<unsigned char>(random());
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            stream << '-';
        stream << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return stream.str();
}
} // namespace luma
