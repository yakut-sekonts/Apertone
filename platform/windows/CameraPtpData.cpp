#include "platform/windows/CameraPtpData.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace luma::windows::ptp {
namespace {
[[noreturn]] void incomplete() {
    throw std::runtime_error("The camera returned an invalid PTP dataset.");
}

class Reader {
  public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    std::uint32_t unsignedValue(unsigned width) {
        if (bytes_.size() < width)
            incomplete();
        std::uint32_t result = 0;
        for (unsigned i = 0; i < width; ++i)
            result |= static_cast<std::uint32_t>(bytes_[i]) << (8 * i);
        bytes_ = bytes_.subspan(width);
        return result;
    }
    std::uint16_t word() {
        return static_cast<std::uint16_t>(unsignedValue(2));
    }
    std::u16string string() {
        const auto count = unsignedValue(1);
        if (!count)
            return {};
        std::u16string text;
        for (std::uint32_t i = 1; i < count; ++i)
            text.push_back(static_cast<char16_t>(word()));
        if (word() != 0)
            incomplete();
        return text;
    }
    std::vector<std::uint16_t> codes() {
        const auto count = unsignedValue(4);
        if (count > 16'384 || bytes_.size() < count * 2)
            incomplete();
        std::vector<std::uint16_t> result;
        result.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
            result.push_back(word());
        return result;
    }
    std::int64_t number(std::uint16_t type) {
        switch (type) {
        case 0x0001:
            return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(unsignedValue(1)));
        case 0x0002:
            return unsignedValue(1);
        case 0x0003:
            return std::bit_cast<std::int16_t>(word());
        case 0x0004:
            return word();
        case 0x0005:
            return std::bit_cast<std::int32_t>(unsignedValue(4));
        case 0x0006:
            return unsignedValue(4);
        default:
            incomplete();
        }
    }
    bool empty() const {
        return bytes_.empty();
    }

  private:
    std::span<const std::uint8_t> bytes_;
};
} // namespace

bool DeviceInfo::supports(std::uint16_t operation) const {
    return std::ranges::find(operations, operation) != operations.end();
}
bool DeviceInfo::hasProperty(std::uint16_t property) const {
    return std::ranges::find(properties, property) != properties.end();
}
bool DeviceInfo::nikon() const {
    const auto prefix = std::u16string_view(manufacturer).substr(0, 5);
    return prefix == u"Nikon" || prefix == u"NIKON";
}

DeviceInfo deviceInfo(std::span<const std::uint8_t> bytes) {
    Reader reader(bytes);
    reader.word();           // Standard version
    reader.unsignedValue(4); // Vendor extension ID
    reader.word();           // Vendor extension version
    reader.string();         // Vendor extension description
    reader.word();           // Functional mode
    DeviceInfo result;
    result.operations = reader.codes();
    reader.codes(); // Events
    result.properties = reader.codes();
    reader.codes(); // Capture formats
    reader.codes(); // Image formats
    result.manufacturer = reader.string();
    return result;
}

ExposureProperty exposureProperty(std::span<const std::uint8_t> bytes, std::uint16_t expected) {
    Reader reader(bytes);
    ExposureProperty result;
    result.id = reader.word();
    result.type = reader.word();
    const auto access = reader.unsignedValue(1);
    if (result.id != expected || access > 1)
        incomplete();
    result.writable = access == 1;
    reader.number(result.type); // Factory default
    result.current = reader.number(result.type);
    const auto form = reader.unsignedValue(1);
    if (form == 1) {
        const auto minimum = reader.number(result.type), maximum = reader.number(result.type),
                   step = reader.number(result.type);
        if (minimum > maximum || step <= 0)
            incomplete();
        if ((maximum - minimum) / step <= 500)
            for (auto value = minimum; value <= maximum; value += step)
                result.choices.push_back(value);
    } else if (form == 2) {
        const auto count = reader.word();
        if (count > 512)
            incomplete();
        result.choices.reserve(count);
        for (unsigned i = 0; i < count; ++i)
            result.choices.push_back(reader.number(result.type));
    } else if (form != 0) {
        incomplete();
    }
    if (!reader.empty())
        incomplete();
    result.writable = result.writable && !result.choices.empty();
    return result;
}

std::vector<std::uint8_t> exposureValue(const ExposureProperty &property, std::int64_t value) {
    if (!property.writable ||
        std::ranges::find(property.choices, value) == property.choices.end() || property.type < 1 ||
        property.type > 6)
        throw std::runtime_error("The selected exposure setting is unavailable on the camera.");
    const auto width = static_cast<unsigned>(1U << ((property.type - 1) / 2));
    std::vector<std::uint8_t> bytes(width);
    const auto encoded = static_cast<std::uint32_t>(value);
    for (unsigned i = 0; i < width; ++i)
        bytes[i] = static_cast<std::uint8_t>(encoded >> (8 * i));
    return bytes;
}
} // namespace luma::windows::ptp
