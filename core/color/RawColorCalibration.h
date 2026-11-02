#pragma once
#include <array>
#include <cstdint>

namespace luma {
struct DngColorProfile {
    double calibrationTemperature = 0;
    // DNG ColorMatrix maps XYZ to camera coordinates. CameraCalibration maps
    // reference-camera coordinates to the individual camera.
    std::array<float, 9> colorMatrix{};
    std::array<float, 9> cameraCalibration{1, 0, 0, 0, 1, 0, 0, 0, 1};
    // ForwardMatrix maps white-balanced camera coordinates to XYZ D50.
    std::array<float, 9> forwardMatrix{};
    bool hasForwardMatrix = false;
};

struct RawColorCalibration {
    std::array<float, 9> cameraToSRGB{1, 0, 0, 0, 1, 0, 0, 0, 1};
    std::array<float, 3> asShotGains{1, 1, 1};
    std::array<float, 3> analogBalance{1, 1, 1};
    std::array<float, 3> asShotNeutral{1, 1, 1};
    std::array<DngColorProfile, 2> dngProfiles{};
    std::uint8_t dngProfileCount = 0;
};
} // namespace luma
