#pragma once
#include <array>

namespace luma {
struct RawColorCalibration {
    std::array<float, 9> cameraToSRGB{1, 0, 0, 0, 1, 0, 0, 0, 1};
    std::array<float, 3> asShotGains{1, 1, 1};
};
} // namespace luma
