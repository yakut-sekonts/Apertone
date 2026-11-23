#include "core/io/TiffDirectory.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace luma {
namespace {
constexpr std::array<std::size_t, 14> sizes{0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8, 4};
void require(bool valid) {
    if (!valid)
        throw std::runtime_error("Invalid TIFF field or offset.");
}
} // namespace

TiffDirectory::TiffDirectory(std::span<const std::uint8_t> bytes, std::uint16_t magic)
    : bytes_(bytes) {
    require(bytes.size() >= 8 && bytes[0] == bytes[1] && (bytes[0] == 'I' || bytes[0] == 'M'));
    little_ = bytes[0] == 'I';
    require(u16(2) == magic);
    const std::size_t directory = u32(4);
    require(directory >= 8);
    const std::size_t count = u16(directory);
    require(count > 0 && count <= 256);
    extent_ = directory + 2 + count * 12 + 4;
    require(extent_ <= bytes_.size());
    fields_.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto at = directory + 2 + i * 12;
        const auto tag = u16(at), type = u16(at + 2);
        const auto units = u32(at + 4);
        require(type > 0 && type < sizes.size() && !has(tag));
        const auto length = sizes[type] * units;
        const std::size_t offset = length <= 4 ? at + 8 : u32(at + 8);
        (void)slice(offset, length);
        extent_ = std::max(extent_, offset + length);
        fields_.push_back({tag, type, units, offset});
    }
}

std::span<const std::uint8_t> TiffDirectory::slice(std::size_t offset, std::size_t size) const {
    require(offset <= bytes_.size() && size <= bytes_.size() - offset);
    return bytes_.subspan(offset, size);
}

std::uint16_t TiffDirectory::u16(std::size_t at) const {
    const auto b = slice(at, 2);
    return static_cast<std::uint16_t>(little_ ? b[0] | b[1] << 8 : b[0] << 8 | b[1]);
}

std::uint32_t TiffDirectory::u32(std::size_t at) const {
    const auto b = slice(at, 4);
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < 4; ++i)
        result = (result << 8) | b[little_ ? 3 - i : i];
    return result;
}

const TiffDirectory::Field *TiffDirectory::find(std::uint16_t tag) const {
    const auto it = std::ranges::find(fields_, tag, &Field::tag);
    return it == fields_.end() ? nullptr : &*it;
}

bool TiffDirectory::has(std::uint16_t tag) const {
    return find(tag) != nullptr;
}

bool TiffDirectory::hasType(std::uint16_t tag, std::uint16_t type) const {
    const auto *field = find(tag);
    return field && field->type == type;
}

std::vector<std::uint32_t> TiffDirectory::integers(std::uint16_t tag, std::size_t limit) const {
    const auto *entry = find(tag);
    if (!entry)
        return {};
    require((entry->type == 1 || entry->type == 3 || entry->type == 4) && entry->count <= limit);
    std::vector<std::uint32_t> result;
    result.reserve(entry->count);
    for (std::size_t i = 0; i < entry->count; ++i) {
        const auto at = entry->offset + i * sizes[entry->type];
        result.push_back(entry->type == 1 ? bytes_[at] : entry->type == 3 ? u16(at) : u32(at));
    }
    return result;
}

std::vector<double> TiffDirectory::numbers(std::uint16_t tag, std::size_t limit) const {
    const auto *entry = find(tag);
    if (!entry)
        return {};
    require((entry->type == 5 || entry->type == 10 || entry->type == 11) && entry->count <= limit);
    std::vector<double> result;
    result.reserve(entry->count);
    for (std::size_t i = 0; i < entry->count; ++i) {
        const auto at = entry->offset + i * sizes[entry->type];
        const auto bits = u32(at);
        double value = 0;
        if (entry->type == 11) {
            value = std::bit_cast<float>(bits);
        } else {
            const auto denominator = u32(at + 4);
            require(denominator != 0);
            value = entry->type == 10 ? static_cast<double>(std::bit_cast<std::int32_t>(bits)) /
                                            std::bit_cast<std::int32_t>(denominator)
                                      : static_cast<double>(bits) / denominator;
        }
        require(std::isfinite(value));
        result.push_back(value);
    }
    return result;
}

std::span<const std::uint8_t> TiffDirectory::text(std::uint16_t tag, std::size_t limit) const {
    const auto *entry = find(tag);
    if (!entry)
        return {};
    require((entry->type == 1 || entry->type == 2 || entry->type == 7) && entry->count <= limit);
    return slice(entry->offset, entry->count);
}
} // namespace luma
