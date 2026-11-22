#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace luma::windows::ptp {
struct DeviceInfo {
    std::vector<std::uint16_t> operations;
    std::vector<std::uint16_t> properties;
    std::u16string manufacturer;
    bool supports(std::uint16_t operation) const;
    bool hasProperty(std::uint16_t property) const;
    bool nikon() const;
};
struct ExposureProperty {
    std::uint16_t id = 0;
    std::uint16_t type = 0;
    bool writable = false;
    std::int64_t current = 0;
    std::vector<std::int64_t> choices;
};
DeviceInfo deviceInfo(std::span<const std::uint8_t> bytes);
ExposureProperty exposureProperty(std::span<const std::uint8_t> bytes, std::uint16_t expected);
std::vector<std::uint8_t> exposureValue(const ExposureProperty &property, std::int64_t value);
} // namespace luma::windows::ptp
