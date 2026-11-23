#include "core/color/DngColorCalibration.h"
// This product includes DNG technology under license by Adobe.
#include "core/color/DngIlluminant.h"
#include "core/color/RGBProfiles.h"
#include "core/color/WhiteBalance.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace luma {
namespace {
using Matrix = ColorMatrix3;

Matrix matrix(const std::array<float, 9> &source) {
    Matrix result{};
    std::copy(source.begin(), source.end(), result.begin());
    return result;
}

std::array<float, 9> floats(const Matrix &source) {
    std::array<float, 9> result{};
    std::transform(source.begin(), source.end(), result.begin(), [](double value) {
        if (!std::isfinite(value) || std::abs(value) > 1000)
            throw std::runtime_error("Invalid resolved DNG color matrix.");
        return static_cast<float>(value);
    });
    return result;
}

Matrix diagonal(const std::array<float, 3> &values) {
    return {values[0], 0, 0, 0, values[1], 0, 0, 0, values[2]};
}

std::array<double, 3> transform(const Matrix &m, const std::array<double, 3> &v) {
    return {m[0] * v[0] + m[1] * v[1] + m[2] * v[2], m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
            m[6] * v[0] + m[7] * v[1] + m[8] * v[2]};
}

std::array<double, 3> xyzWhite(DngChromaticity xy) {
    return {xy[0] / xy[1], 1, (1 - xy[0] - xy[1]) / xy[1]};
}

DngChromaticity chromaticity(const std::array<double, 3> &xyz) {
    const auto total = xyz[0] + xyz[1] + xyz[2];
    if (!std::isfinite(total) || total <= 0 || xyz[0] <= 0 || xyz[1] <= 0 || xyz[2] <= 0)
        throw std::runtime_error("Invalid DNG source white point.");
    return {xyz[0] / total, xyz[1] / total};
}

Matrix normalizedForward(const DngColorProfile &profile) {
    auto result = matrix(profile.forwardMatrix);
    const auto target = xyzWhite(dngStandardIlluminant(23));
    const auto white = transform(result, {1, 1, 1});
    for (std::size_t row = 0; row < white.size(); ++row) {
        if (!std::isfinite(white[row]) || white[row] <= 1e-8)
            throw std::runtime_error("Invalid DNG forward matrix white point.");
        for (std::size_t column = 0; column < 3; ++column)
            result[row * 3 + column] *= target[row] / white[row];
    }
    return result;
}

Matrix interpolate(const Matrix &a, const Matrix &b, double weight) {
    Matrix result{};
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = a[i] + weight * (b[i] - a[i]);
    return result;
}

struct InterpolatedProfile {
    Matrix referenceToCamera{};
    Matrix cameraMatrix{};
    Matrix forwardMatrix{};
    bool hasForwardMatrix = false;
    DngIlluminantWeights weights{};
};

InterpolatedProfile tripleProfileAt(const RawColorCalibration &calibration, DngChromaticity xy) {
    const auto &profiles = calibration.dngProfiles;
    InterpolatedProfile result;
    result.weights =
        dngTripleIlluminantWeights(xy, {profiles[0].calibrationWhite, profiles[1].calibrationWhite,
                                        profiles[2].calibrationWhite});
    Matrix color{};
    result.hasForwardMatrix = profiles[0].hasForwardMatrix;
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        const auto &profile = profiles[i];
        auto forward = result.hasForwardMatrix ? normalizedForward(profile) : Matrix{};
        for (std::size_t j = 0; j < color.size(); ++j) {
            color[j] += result.weights[i] * profile.colorMatrix[j];
            result.forwardMatrix[j] += result.weights[i] * forward[j];
        }
    }
    // This external-profile path uses the camera reference space, where the
    // per-unit calibration is identity and only analog balance remains.
    result.referenceToCamera = diagonal(calibration.analogBalance);
    result.cameraMatrix = multiplyColorMatrices(result.referenceToCamera, color);
    (void)inverseColorMatrix(result.cameraMatrix);
    if (result.hasForwardMatrix)
        (void)inverseColorMatrix(result.forwardMatrix);
    return result;
}

InterpolatedProfile profileAt(const RawColorCalibration &calibration, double temperature,
                              DngChromaticity xy = {}) {
    if (calibration.dngProfileCount == 3)
        return tripleProfileAt(calibration, xy);
    if (calibration.dngProfileCount == 0 || calibration.dngProfileCount > 2)
        throw std::runtime_error("DNG color profile is missing.");
    const auto &first = calibration.dngProfiles[0];
    const auto &second = calibration.dngProfiles[calibration.dngProfileCount == 2 ? 1 : 0];
    double weight = 0;
    if (calibration.dngProfileCount == 2) {
        const double firstMired = 1'000'000 / first.calibrationTemperature;
        const double secondMired = 1'000'000 / second.calibrationTemperature;
        const double targetMired = 1'000'000 / temperature;
        if (!std::isfinite(firstMired) || !std::isfinite(secondMired) ||
            std::abs(secondMired - firstMired) < 1e-9)
            throw std::runtime_error("DNG calibration illuminants are invalid.");
        weight = std::clamp((targetMired - firstMired) / (secondMired - firstMired), 0.0, 1.0);
    }
    const auto color = interpolate(matrix(first.colorMatrix), matrix(second.colorMatrix), weight);
    const auto cameraCalibration =
        interpolate(matrix(first.cameraCalibration), matrix(second.cameraCalibration), weight);
    const auto analog = diagonal(calibration.analogBalance);
    InterpolatedProfile result;
    result.referenceToCamera = multiplyColorMatrices(analog, cameraCalibration);
    result.cameraMatrix = multiplyColorMatrices(result.referenceToCamera, color);
    (void)inverseColorMatrix(result.referenceToCamera);
    (void)inverseColorMatrix(result.cameraMatrix);
    result.hasForwardMatrix = first.hasForwardMatrix && second.hasForwardMatrix;
    if (result.hasForwardMatrix) {
        result.forwardMatrix =
            interpolate(matrix(first.forwardMatrix), matrix(second.forwardMatrix), weight);
        (void)inverseColorMatrix(result.forwardMatrix);
    }
    return result;
}

std::array<float, 3> gainsFor(const std::array<double, 3> &neutral) {
    for (double value : neutral)
        if (!std::isfinite(value) || value <= 1e-8)
            throw std::runtime_error("DNG profile cannot represent the requested white balance.");
    return {static_cast<float>(neutral[1] / neutral[0]), 1,
            static_cast<float>(neutral[1] / neutral[2])};
}

Matrix bradfordAdaptation(const std::array<double, 3> &source) {
    constexpr Matrix bradford{0.8951, 0.2664, -0.1614, -0.7502, 1.7135,
                              0.0367, 0.0389, -0.0685, 1.0296};
    constexpr std::array<double, 3> d50{0.9642, 1, 0.8249};
    const auto sourceLms = transform(bradford, source);
    const auto targetLms = transform(bradford, d50);
    for (double value : sourceLms)
        if (!std::isfinite(value) || value <= 1e-8)
            throw std::runtime_error("Invalid DNG source white point.");
    const Matrix scale{targetLms[0] / sourceLms[0], 0, 0, 0, targetLms[1] / sourceLms[1], 0, 0, 0,
                       targetLms[2] / sourceLms[2]};
    return multiplyColorMatrices(inverseColorMatrix(bradford),
                                 multiplyColorMatrices(scale, bradford));
}

ResolvedRawColorCalibration resolveProfile(const InterpolatedProfile &profile,
                                           const std::array<double, 3> &whitePoint,
                                           const std::array<float, 3> *cameraNeutralOverride) {
    std::array<double, 3> cameraNeutral{};
    if (cameraNeutralOverride)
        std::copy(cameraNeutralOverride->begin(), cameraNeutralOverride->end(),
                  cameraNeutral.begin());
    else
        cameraNeutral = transform(profile.cameraMatrix, whitePoint);
    ResolvedRawColorCalibration result;
    result.gains = gainsFor(cameraNeutral);
    result.profileWeights = profile.weights;
    static const Matrix xyzD50ToSRGB = inverseColorMatrix(rgbToXYZD50(linearSRGBProfile()));
    if (profile.hasForwardMatrix) {
        const auto cameraToReference = inverseColorMatrix(profile.referenceToCamera);
        const auto referenceNeutral = transform(cameraToReference, cameraNeutral);
        std::array<float, 3> inverseReferenceNeutral{};
        for (std::size_t i = 0; i < inverseReferenceNeutral.size(); ++i) {
            if (!std::isfinite(referenceNeutral[i]) || referenceNeutral[i] <= 1e-8)
                throw std::runtime_error("DNG reference neutral is invalid.");
            inverseReferenceNeutral[i] = static_cast<float>(1 / referenceNeutral[i]);
        }
        const auto cameraToD50 = multiplyColorMatrices(
            profile.forwardMatrix,
            multiplyColorMatrices(diagonal(inverseReferenceNeutral), cameraToReference));
        std::array<float, 3> inverseGains{};
        for (std::size_t i = 0; i < inverseGains.size(); ++i)
            inverseGains[i] = 1 / result.gains[i];
        const auto postGainCameraToD50 = multiplyColorMatrices(cameraToD50, diagonal(inverseGains));
        result.cameraToSRGB = floats(multiplyColorMatrices(xyzD50ToSRGB, postGainCameraToD50));
    } else {
        const auto balancedCamera =
            multiplyColorMatrices(diagonal(result.gains), profile.cameraMatrix);
        const auto cameraToSourceXYZ = inverseColorMatrix(balancedCamera);
        auto sourceWhite = transform(cameraToSourceXYZ, std::array<double, 3>{1, 1, 1});
        if (!std::isfinite(sourceWhite[1]) || sourceWhite[1] <= 1e-8)
            throw std::runtime_error("DNG source white point is invalid.");
        const double sourceWhiteY = sourceWhite[1];
        for (double &value : sourceWhite)
            value /= sourceWhiteY;
        result.cameraToSRGB = floats(multiplyColorMatrices(
            xyzD50ToSRGB,
            multiplyColorMatrices(bradfordAdaptation(sourceWhite), cameraToSourceXYZ)));
    }
    return result;
}

ResolvedRawColorCalibration
resolveDng(const RawColorCalibration &calibration, double temperature, double tint,
           const std::array<float, 3> *cameraNeutralOverride = nullptr) {
    if (!std::isfinite(temperature) || temperature < 2000 || temperature > 25000 ||
        !std::isfinite(tint) || tint < -150 || tint > 150)
        throw std::runtime_error("DNG white balance is outside the supported range.");
    if (calibration.dngProfileCount == 3) {
        const auto xy = dngWhiteChromaticity(temperature, tint);
        return resolveProfile(profileAt(calibration, temperature, xy), xyzWhite(xy),
                              cameraNeutralOverride);
    }
    return resolveProfile(profileAt(calibration, temperature), whitePointXYZ(temperature, tint),
                          cameraNeutralOverride);
}

void finalizeTriple(RawColorCalibration &calibration) {
    std::array<double, 3> neutral{};
    std::copy(calibration.asShotNeutral.begin(), calibration.asShotNeutral.end(), neutral.begin());
    auto xy = dngStandardIlluminant(23);
    // Iterate from D50 using the full white chromaticity, including tint.
    // The midpoint of the last and current estimates handles two-cycle cases.
    for (int iteration = 0; iteration < 30; ++iteration) {
        const auto profile = profileAt(calibration, 0, xy);
        auto next = chromaticity(transform(inverseColorMatrix(profile.cameraMatrix), neutral));
        const auto delta = std::abs(next[0] - xy[0]) + std::abs(next[1] - xy[1]);
        if (iteration == 29 && delta >= 1e-7)
            next = {std::midpoint(xy[0], next[0]), std::midpoint(xy[1], next[1])};
        xy = next;
        if (delta < 1e-7)
            break;
    }
    const auto resolved =
        resolveProfile(profileAt(calibration, 0, xy), xyzWhite(xy), &calibration.asShotNeutral);
    calibration.cameraToSRGB = resolved.cameraToSRGB;
    calibration.asShotGains = resolved.gains;
    calibration.asShotTemperature = dngWhiteTemperatureTint(xy)[0];
    calibration.asShotProfileWeights = resolved.profileWeights;
}
} // namespace

std::optional<double> dngIlluminantTemperature(unsigned illuminant) {
    // TIFF/EP LightSource values used by CalibrationIlluminant. Values follow
    // the DNG specification's standard illuminants; fluorescent families use
    // their nominal CCT and remain bounded by the profile matrices themselves.
    switch (illuminant) {
    case 1:
        return 5500; // Daylight
    case 2:
        return 4200; // Fluorescent
    case 3:
        return 2850; // Tungsten
    case 4:
        return 5500; // Flash
    case 9:
        return 6500; // Fine weather
    case 10:
        return 7500; // Cloudy
    case 11:
        return 8000; // Shade
    case 12:
        return 6430; // Daylight fluorescent
    case 13:
        return 5000; // Day white fluorescent
    case 14:
        return 4230; // Cool white fluorescent
    case 15:
        return 3450; // White fluorescent
    case 17:
        return 2856; // Standard light A
    case 18:
        return 4874; // Standard light B
    case 19:
        return 6774; // Standard light C
    case 20:
        return 5503; // D55
    case 21:
        return 6504; // D65
    case 22:
        return 7504; // D75
    case 23:
        return 5003; // D50
    case 24:
        return 3200; // ISO studio tungsten
    default:
        return std::nullopt;
    }
}

ResolvedRawColorCalibration resolveRawColorCalibration(const RawColorCalibration &calibration,
                                                       double temperature, double tint) {
    if (calibration.dngProfileCount != 0)
        return resolveDng(calibration, temperature, tint);
    return {calibration.cameraToSRGB, whiteBalanceGains(temperature, tint, calibration)};
}

void finalizeDngColorCalibration(RawColorCalibration &calibration) {
    if (calibration.dngProfileCount == 0)
        return;
    if (calibration.dngProfileCount > calibration.dngProfiles.size())
        throw std::runtime_error("DNG contains too many color profiles.");
    for (std::size_t i = 0; i < calibration.dngProfileCount; ++i) {
        const auto &profile = calibration.dngProfiles[i];
        if (calibration.dngProfileCount < 3 &&
            (!std::isfinite(profile.calibrationTemperature) ||
             profile.calibrationTemperature < 2000 || profile.calibrationTemperature > 25000))
            throw std::runtime_error("Unsupported DNG calibration illuminant.");
        (void)inverseColorMatrix(matrix(profile.colorMatrix));
        (void)inverseColorMatrix(matrix(profile.cameraCalibration));
        if (profile.hasForwardMatrix)
            (void)inverseColorMatrix(matrix(profile.forwardMatrix));
    }
    for (float value : calibration.analogBalance)
        if (!std::isfinite(value) || value <= 0)
            throw std::runtime_error("Invalid DNG AnalogBalance.");
    for (float value : calibration.asShotNeutral)
        if (!std::isfinite(value) || value <= 0)
            throw std::runtime_error("Invalid DNG AsShotNeutral.");

    if (calibration.dngProfileCount == 3) {
        finalizeTriple(calibration);
        return;
    }

    const double targetRed = std::log(calibration.asShotNeutral[0] / calibration.asShotNeutral[1]);
    const double targetBlue = std::log(calibration.asShotNeutral[2] / calibration.asShotNeutral[1]);
    double bestError = std::numeric_limits<double>::infinity();
    double bestTemperature = 6500;
    // The profile interpolation itself is in reciprocal temperature, so search
    // uniformly in mireds and retain the exact AsShotNeutral gains afterwards.
    for (int i = 0; i <= 1024; ++i) {
        const double mired = 40 + (500 - 40) * i / 1024.0;
        const double temperature = 1'000'000 / mired;
        const auto predicted = transform(profileAt(calibration, temperature).cameraMatrix,
                                         whitePointXYZ(temperature, 0));
        if (predicted[0] <= 0 || predicted[1] <= 0 || predicted[2] <= 0)
            continue;
        const double red = std::log(predicted[0] / predicted[1]) - targetRed;
        const double blue = std::log(predicted[2] / predicted[1]) - targetBlue;
        const double error = red * red + blue * blue;
        if (error < bestError) {
            bestError = error;
            bestTemperature = temperature;
        }
    }
    if (!std::isfinite(bestError))
        throw std::runtime_error("DNG AsShotNeutral does not match its color profile.");
    const auto resolved = resolveDng(calibration, bestTemperature, 0, &calibration.asShotNeutral);
    calibration.cameraToSRGB = resolved.cameraToSRGB;
    calibration.asShotGains = resolved.gains;
    calibration.asShotTemperature = bestTemperature;
}
} // namespace luma
