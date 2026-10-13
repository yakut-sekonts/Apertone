#pragma once
#include "core/edits/LensSettings.h"
#include "core/metadata/PhotoMetadata.h"
#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace luma {
struct LensCalibration {
    std::string_view model;
    double focal = 0;
    double aperture = 0;
    double distance = 0;
    std::array<double, 6> terms{};
};
struct LensProfile {
    std::string_view id, name;
    std::span<const std::string_view> aliases, mounts;
    double cropFactor = 1;
    double aspect = 1.5;
    double centerX = 0, centerY = 0;
    std::span<const LensCalibration> distortion, tca, vignetting;
};
struct LensCamera {
    std::string_view name;
    std::span<const std::string_view> aliases;
    std::string_view mount;
    double cropFactor = 1;
};
struct LensMount {
    std::string_view name;
    std::span<const std::string_view> compatible;
};
struct ResolvedLens {
    const LensCamera *camera = nullptr;
    const LensProfile *profile = nullptr;
    std::optional<LensCalibration> distortion, tca, vignetting;
    std::string status;
};
std::span<const LensProfile> lensProfiles();
std::span<const LensCamera> lensCameras();
const LensProfile *lensProfile(std::string_view id);
bool compatibleLens(const LensCamera &camera, const LensProfile &profile);
ResolvedLens resolveLens(const PhotoMetadata &metadata, const LensSettings &settings);
} // namespace luma
