#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace luma {
// Classic TIFF and the TIFF-derived DNG camera-profile container use the same
// bounded first directory. Offsets remain relative to the supplied bytes.
class TiffDirectory {
  public:
    explicit TiffDirectory(std::span<const std::uint8_t> bytes, std::uint16_t magic = 42);
    bool has(std::uint16_t tag) const;
    std::vector<std::uint32_t> integers(std::uint16_t tag, std::size_t limit) const;
    std::vector<double> numbers(std::uint16_t tag, std::size_t limit) const;
    std::span<const std::uint8_t> text(std::uint16_t tag, std::size_t limit) const;
    std::span<const std::uint8_t> slice(std::size_t offset, std::size_t size) const;
    std::size_t extent() const {
        return extent_;
    }

  private:
    struct Field {
        std::uint16_t tag, type;
        std::uint32_t count;
        std::size_t offset;
    };
    const Field *find(std::uint16_t tag) const;
    std::uint16_t u16(std::size_t at) const;
    std::uint32_t u32(std::size_t at) const;
    std::span<const std::uint8_t> bytes_;
    std::vector<Field> fields_;
    bool little_ = true;
    std::size_t extent_ = 0;
};
} // namespace luma
