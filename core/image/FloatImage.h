#pragma once
#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace luma {
inline std::size_t pixelCount(int width, int height) {
    if (width <= 0 || height <= 0 || width > 20000 || height > 20000 ||
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) > 100'000'000) {
        throw std::runtime_error("Image dimensions exceed the supported memory budget (100 MP).");
    }
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
}

/// Scene-linear sRGB; negative values and highlights above 1 are preserved.
struct FloatImage {
    int width;
    int height;
    std::vector<float> rgb;
    FloatImage(int w, int h) : width(w), height(h), rgb(pixelCount(w, h) * 3) {}
};

struct BayerFrame {
    int width;
    int height;
    std::array<int, 4> pattern;
    std::vector<float> samples;
    BayerFrame(int w, int h, std::array<int, 4> cfa)
        : width(w), height(h), pattern(cfa), samples(pixelCount(w, h)) {}
    int colorAt(int x, int y) const {
        return pattern[(y & 1) * 2 + (x & 1)];
    }
};
} // namespace luma
