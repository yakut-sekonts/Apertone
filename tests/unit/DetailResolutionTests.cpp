#include "core/export/DetailResolution.h"
#include "core/render/NoiseProfile.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}

luma::LinearOutput blurredEdge() {
    luma::LinearOutput image(15, 9);
    for (int y = 0; y < image.height; ++y)
        for (int x = 0; x < image.width; ++x) {
            const double value = x < 6 ? .2 : x == 6 ? .3 : x == 7 ? .5 : x == 8 ? .7 : .8;
            const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 4;
            for (std::size_t channel = 0; channel < 3; ++channel)
                image.rgba[offset + channel] = static_cast<float>(value);
            image.rgba[offset + 3] = 1;
        }
    return image;
}

double luminance(const luma::LinearOutput &image, int x, int y) {
    const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 4;
    return .2126 * image.rgba[offset] + .7152 * image.rgba[offset + 1] +
           .0722 * image.rgba[offset + 2];
}

double chromaEnergy(const luma::LinearOutput &image) {
    double result = 0;
    for (std::size_t offset = 0; offset < image.rgba.size(); offset += 4) {
        const double value = .2126 * image.rgba[offset] + .7152 * image.rgba[offset + 1] +
                             .0722 * image.rgba[offset + 2];
        result += std::hypot(image.rgba[offset] - value, image.rgba[offset + 2] - value);
    }
    return result;
}

luma::FloatImage noiseFixture() {
    luma::FloatImage image(128, 128);
    for (int y = 0; y < image.height; ++y)
        for (int x = 0; x < image.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * image.width + x;
            const double luminanceNoise = ((x * 17 + y * 29) % 19 - 9) * .0009;
            const double colorNoise = ((x * 31 + y * 13) % 17 - 8) * .0007;
            image.rgb[index * 3] = static_cast<float>(.25 + luminanceNoise + colorNoise);
            image.rgb[index * 3 + 1] = static_cast<float>(.25 + luminanceNoise);
            image.rgb[index * 3 + 2] = static_cast<float>(.25 + luminanceNoise - colorNoise);
        }
    return image;
}
} // namespace

int main() {
    try {
        auto deblurred = blurredEdge();
        const double before = luminance(deblurred, 8, 4) - luminance(deblurred, 6, 4);
        luma::recoverLensBlur(deblurred, .8);
        const double after = luminance(deblurred, 8, 4) - luminance(deblurred, 6, 4);
        require(after > before, "Lens blur recovery did not increase bounded edge separation.");

        luma::LinearOutput moire(16, 16);
        for (int y = 0; y < moire.height; ++y)
            for (int x = 0; x < moire.width; ++x) {
                const auto offset = (static_cast<std::size_t>(y) * moire.width + x) * 4;
                const double chroma = (x + y) % 2 ? .12 : -.12;
                const double red = .4 + chroma, blue = .4 - chroma;
                const double green = (.4 - .2126 * red - .0722 * blue) / .7152;
                moire.rgba[offset] = static_cast<float>(red);
                moire.rgba[offset + 1] = static_cast<float>(green);
                moire.rgba[offset + 2] = static_cast<float>(blue);
                moire.rgba[offset + 3] = 1;
            }
        const auto originalMoire = moire;
        const double beforeEnergy = chromaEnergy(moire);
        luma::suppressColorMoire(moire, 1);
        require(chromaEnergy(moire) < beforeEnergy * .85,
                "Color moire correction did not suppress chroma oscillation.");
        for (int y = 0; y < moire.height; ++y)
            for (int x = 0; x < moire.width; ++x)
                require(std::abs(luminance(moire, x, y) - luminance(originalMoire, x, y)) < 1e-6,
                        "Color moire correction changed luminance.");

        auto second = originalMoire;
        luma::suppressColorMoire(second, 1);
        require(second.rgba == moire.rgba, "Color moire correction is not deterministic.");

        const auto noise = noiseFixture();
        luma::PhotoMetadata lowMetadata, highMetadata;
        lowMetadata.iso = 100;
        highMetadata.iso = 12800;
        const auto low = luma::estimateNoiseProfile(noise, lowMetadata);
        const auto high = luma::estimateNoiseProfile(noise, highMetadata);
        require(low.measurementAvailable && low.sampleCount >= 64,
                "Noise profile did not use measurable flat samples.");
        require(high.luminance > low.luminance && high.color > low.color,
                "ISO prior did not conservatively increase the measured noise profile.");
        require(luma::estimateNoiseProfile(noise, lowMetadata).luminance == low.luminance,
                "Noise profile is not deterministic.");

        luma::CancellationSource cancelled;
        cancelled.cancel();
        bool refused = false;
        try {
            auto image = blurredEdge();
            luma::recoverLensBlur(image, .5, cancelled.token());
        } catch (const std::exception &) {
            refused = true;
        }
        require(refused, "Detail recovery ignored cancellation.");
        std::cout << "Deblur, color moire, noise profile and cancellation contracts passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
