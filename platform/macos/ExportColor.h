#pragma once
#include "core/color/RGBProfiles.h"
#include "core/export/ExportOptions.h"

namespace luma::macos {
class ExportColor final {
  public:
    explicit ExportColor(ExportColorSpace space);
    ~ExportColor();
    ExportColor(const ExportColor &) = delete;
    ExportColor &operator=(const ExportColor &) = delete;
    const std::vector<std::uint8_t> &profile() const {
        return profile_;
    }
    const ColorMatrix3 &colorants() const {
        return colorants_;
    }
    void convert(std::span<const float> rgb, std::span<float> output, bool linear) const;

  private:
    std::vector<std::uint8_t> profile_;
    ColorMatrix3 colorants_{};
    void *encoded_ = nullptr, *linear_ = nullptr;
};
} // namespace luma::macos
