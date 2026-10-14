#pragma once
#include <array>
#include <cmath>
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

/// Linear sRGB working pixels; negative values and highlights above 1 are preserved.
struct FloatImage {
    int width;
    int height;
    std::vector<float> rgb;
    // Straight coverage, empty for opaque inputs (the exact legacy RAW path).
    std::vector<float> alpha;
    void validateCoverage() const {
        const auto count = pixelCount(width, height);
        if (rgb.size() != count * 3 || (!alpha.empty() && alpha.size() != count))
            throw std::runtime_error("Invalid image color/coverage buffers.");
        for (const auto value : alpha)
            if (!std::isfinite(value) || value < 0 || value > 1)
                throw std::runtime_error("Invalid image coverage.");
    }
    float coverage(int x, int y) const {
        return alpha.empty() ? 1 : alpha[static_cast<std::size_t>(y) * width + x];
    }
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
