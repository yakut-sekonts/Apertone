#pragma once
#include "core/color/RawColorCalibration.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace luma {
struct DcpHueTable {
    std::array<unsigned, 3> divisions{};
    bool gammaEncodedValue = false;
    std::vector<float> samples;
};
struct DcpProfile {
    std::string source, model, name;
    std::array<DngColorProfile, 2> illuminants;
    unsigned illuminantCount = 0;
    std::array<DcpHueTable, 2> hueSaturation;
    DcpHueTable look;
    std::vector<std::array<float, 2>> tone;
    double baselineExposure = 0;
};
struct DcpSelection {
    std::string digest;
    std::shared_ptr<const DcpProfile> profile;
    bool operator==(const DcpSelection &other) const {
        return digest == other.digest;
    }
};
inline constexpr std::size_t maximumDcpBytes = 4 * 1024 * 1024;

std::shared_ptr<const DcpProfile> readDcpProfile(std::string source);
RawColorCalibration applyDcpCalibration(RawColorCalibration calibration, const DcpProfile &profile);
std::array<float, 3> applyDcpHueSaturation(const DcpProfile &profile, std::array<float, 3> rgb,
                                           double temperature);
std::array<float, 3> applyDcpLook(const DcpProfile &profile, std::array<float, 3> rgb);
} // namespace luma
