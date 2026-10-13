#include "core/lens/LensProfiles.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace luma {
namespace {
#include "LensDatabase.h"
std::string normalized(std::string_view value) {
    std::string result;
    for (char c : value) {
        if (c >= 'A' && c <= 'Z')
            result += static_cast<char>(c - 'A' + 'a');
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            result += c;
    }
    return result;
}
bool matches(std::string_view value, std::span<const std::string_view> aliases) {
    const auto key = normalized(value);
    return !key.empty() && std::any_of(aliases.begin(), aliases.end(),
                                       [&](auto alias) { return key == normalized(alias); });
}
std::optional<LensCalibration> interpolate(const std::vector<LensCalibration> &data, double value,
                                           bool aperture = false) {
    const auto coordinate = [aperture](const auto &sample) {
        return aperture ? sample.aperture : sample.focal;
    };
    const LensCalibration *lower = nullptr, *upper = nullptr;
    for (const auto &sample : data) {
        const auto at = coordinate(sample);
        if (std::abs(at - value) <= 1e-6 * value)
            return sample;
        if (at < value && (!lower || at > coordinate(*lower)))
            lower = &sample;
        if (at > value && (!upper || at < coordinate(*upper)))
            upper = &sample;
    }
    if (!lower || !upper || lower->model != upper->model)
        return {};
    auto result = *lower;
    const auto fraction = (value - coordinate(*lower)) / (coordinate(*upper) - coordinate(*lower));
    for (std::size_t i = 0; i < result.terms.size(); ++i)
        result.terms[i] = std::lerp(lower->terms[i], upper->terms[i], fraction);
    if (aperture)
        result.aperture = value;
    else
        result.focal = value;
    return result;
}
std::optional<LensCalibration> vignette(const LensProfile &profile, double focal, double aperture) {
    // Resolve farthest distance separately for each calibrated focal/aperture.
    std::map<std::pair<double, double>, LensCalibration> farthest;
    for (const auto &sample : profile.vignetting) {
        const auto key = std::pair{sample.focal, sample.aperture};
        const auto found = farthest.find(key);
        if (found == farthest.end() || found->second.distance < sample.distance)
            farthest[key] = sample;
    }
    std::map<double, std::vector<LensCalibration>> byFocal;
    for (const auto &[key, sample] : farthest)
        byFocal[key.first].push_back(sample);
    std::vector<LensCalibration> atAperture;
    for (const auto &[length, samples] : byFocal)
        if (auto value = interpolate(samples, aperture, true))
            atAperture.push_back(*value);
    return interpolate(atAperture, focal);
}
} // namespace
std::span<const LensProfile> lensProfiles() {
    return profiles;
}
std::span<const LensCamera> lensCameras() {
    return cameras;
}
const LensProfile *lensProfile(std::string_view id) {
    const auto found = std::find_if(profiles.begin(), profiles.end(),
                                    [&](const auto &profile) { return profile.id == id; });
    return found == profiles.end() ? nullptr : &*found;
}
bool compatibleLens(const LensCamera &camera, const LensProfile &profile) {
    // A calibration made on a smaller sensor cannot cover a larger sensor.
    if (profile.cropFactor > camera.cropFactor * 1.01)
        return false;
    std::set<std::string_view> visited;
    std::vector<std::string_view> pending{camera.mount};
    while (!pending.empty()) {
        const auto name = pending.back();
        pending.pop_back();
        if (!visited.insert(name).second)
            continue;
        if (std::find(profile.mounts.begin(), profile.mounts.end(), name) != profile.mounts.end())
            return true;
        for (const auto &mount : mounts)
            if (mount.name == name)
                pending.insert(pending.end(), mount.compatible.begin(), mount.compatible.end());
    }
    return false;
}
ResolvedLens resolveLens(const PhotoMetadata &metadata, const LensSettings &settings) {
    settings.validate();
    ResolvedLens result;
    for (const auto &camera : cameras)
        if (matches(metadata.camera, camera.aliases)) {
            if (result.camera && (camera.mount != result.camera->mount ||
                                  camera.cropFactor != result.camera->cropFactor)) {
                result.camera = nullptr;
                result.status = "Неоднозначная camera; профиль не применяется";
                return result;
            }
            result.camera = &camera;
        }
    if (!result.camera) {
        result.status = "Camera отсутствует в базе; профиль не применяется";
        return result;
    }
    if (!settings.profile.empty()) {
        result.profile = lensProfile(settings.profile);
        if (!result.profile || !compatibleLens(*result.camera, *result.profile)) {
            result.profile = nullptr;
            result.status = "Выбранный профиль отсутствует или несовместим с camera";
            return result;
        }
    } else {
        for (const auto &profile : profiles)
            if (matches(metadata.lens, profile.aliases) &&
                compatibleLens(*result.camera, profile)) {
                if (result.profile) {
                    result.profile = nullptr;
                    result.status = "Неоднозначный lens; выберите профиль вручную";
                    return result;
                }
                result.profile = &profile;
            }
    }
    if (!result.profile) {
        result.status = "Lens не определён; выберите профиль вручную";
        return result;
    }
    const double focal = metadata.focalLength.value_or(0);
    const double aperture = metadata.aperture.value_or(0);
    if (!std::isfinite(focal) || focal <= 0) {
        result.status = "Нет focal length; профиль не применяется";
        return result;
    }
    const auto &profile = *result.profile;
    result.distortion = interpolate({profile.distortion.begin(), profile.distortion.end()}, focal);
    result.tca = interpolate({profile.tca.begin(), profile.tca.end()}, focal);
    if (std::isfinite(aperture) && aperture > 0)
        result.vignetting = vignette(profile, focal, aperture);
    result.status = "Lensfun 0.3.4 · ";
    result.status += result.distortion ? "Distortion ✓" : "Distortion: нет калибровки";
    result.status += result.tca ? " · CA ✓" : " · CA: нет калибровки";
    result.status += result.vignetting ? " · Vignetting ✓ (дальняя фокусировка)"
                                       : " · Vignetting: нет калибровки для focal/aperture";
    return result;
}
} // namespace luma
