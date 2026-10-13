#include "core/render/CpuRenderer.h"
#include "core/render/CropGeometry.h"
#include <algorithm>
#include <iostream>
#include <limits>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void rejects(F action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Expected crop validation failure.");
}
std::array<float, 3> pixel(int x, int y) {
    return {static_cast<float>(x), static_cast<float>(y), -2.F};
}
void transforms() {
    for (int orientation = 0; orientation < 8; ++orientation)
        for (int turn = 0; turn < 4; ++turn)
            for (int flips = 0; flips < 4; ++flips) {
                luma::EditDocument edits;
                edits.crop.quarterTurns = turn;
                edits.crop.flipHorizontal = flips & 1;
                edits.crop.flipVertical = flips & 2;
                const int uw = orientation & 4 ? 6 : 8, uh = orientation & 4 ? 8 : 6;
                const int w = turn % 2 ? uh : uw, h = turn % 2 ? uw : uh;
                const luma::CropGeometry geometry(8, 6, orientation, edits);
                require(geometry.width() == w && geometry.height() == h,
                        "Quarter rotation dimensions wrong.");
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x) {
                        int tx = flips & 1 ? w - 1 - x : x, ty = flips & 2 ? h - 1 - y : y;
                        int ux = tx, uy = ty;
                        if (turn == 1) {
                            ux = ty;
                            uy = uh - 1 - tx;
                        } else if (turn == 2) {
                            ux = uw - 1 - tx;
                            uy = uh - 1 - ty;
                        } else if (turn == 3) {
                            ux = uw - 1 - ty;
                            uy = tx;
                        }
                        int sx = orientation & 4 ? uy : ux, sy = orientation & 4 ? ux : uy;
                        if (orientation & 1)
                            sx = 7 - sx;
                        if (orientation & 2)
                            sy = 5 - sy;
                        require(geometry.sample(x, y, pixel) == pixel(sx, sy),
                                "Exact orientation/quarter/flip pixels differ.");
                    }
            }
    luma::EditDocument edits;
    edits.crop.x = 0.25;
    edits.crop.y = 1.0 / 6;
    edits.crop.width = 0.5;
    edits.crop.height = 0.5;
    const luma::CropGeometry cropped(8, 6, 0, edits);
    require(cropped.width() == 4 && cropped.height() == 3 &&
                cropped.sample(0, 0, pixel) == pixel(2, 1) &&
                cropped.sample(3, 2, pixel) == pixel(5, 3),
            "Crop pixel edges are wrong.");
    edits.setEnabled(luma::Correction::crop, false);
    const luma::CropGeometry off(8, 6, 0, edits);
    require(off.width() == 8 && off.height() == 6 && off.sample(0, 0, pixel) == pixel(0, 0),
            "Disabled crop changed pixels.");
    luma::FloatImage image(8, 6);
    for (std::size_t i = 0; i < image.rgb.size(); ++i)
        image.rgb[i] = static_cast<float>(i % 23) / 25;
    const auto base = luma::renderCPU(image, {});
    edits.setEnabled(luma::Correction::crop, true);
    edits.crop = {};
    edits.crop.quarterTurns = 1;
    const auto rotated = luma::renderCPU(image, edits);
    for (int y = 0; y < rotated.height; ++y)
        for (int x = 0; x < rotated.width; ++x)
            for (std::size_t c = 0; c < 4; ++c)
                require(rotated.rgba[(static_cast<std::size_t>(y) * rotated.width + x) * 4 + c] ==
                            base.rgba[(static_cast<std::size_t>(5 - x) * 8 + y) * 4 + c],
                        "Display rotation changed quantized pixels.");
}
void ratiosAndStraighten() {
    for (double ratio : {-1., 1., 4. / 3, 3. / 2, 16. / 9, 5. / 4, 0.01, 100.}) {
        luma::EditDocument edits;
        edits.crop.aspect = ratio;
        edits.crop.width = 0.8;
        edits.crop.height = 0.7;
        edits.crop.x = 0.1;
        edits.crop.y = 0.15;
        const luma::CropGeometry geometry(800, 600, 0, edits);
        const double wanted = ratio == -1 ? 4. / 3 : ratio;
        require(std::abs(geometry.width() - wanted * geometry.height()) <= 1 + wanted,
                "Locked ratio not respected after pixel rounding.");
        const auto frame = geometry.frame();
        require(frame[0] >= 0.1 - 1. / 800 && frame[1] >= 0.15 - 1. / 600 &&
                    frame[0] + frame[2] <= 0.9 + 1. / 800 && frame[1] + frame[3] <= 0.85 + 1. / 600,
                "Aspect crop exceeds proposal.");
    }
    luma::EditDocument square;
    square.crop = {0.1, 0.123, 0.432, 0.678, 0, 1, 0, false, false};
    const luma::CropGeometry squarePixels(89, 61, 0, square);
    require(squarePixels.width() == squarePixels.height(),
            "Square crop gained a pixel from independent edge rounding.");
    for (int orientation = 0; orientation < 8; ++orientation)
        for (double angle : {-45., -20., 0.01, 20., 45.}) {
            luma::EditDocument edits;
            edits.crop.angle = angle;
            edits.crop.quarterTurns = 1;
            edits.crop.flipHorizontal = true;
            const luma::CropGeometry geometry(80, 60, orientation, edits);
            for (int y : {0, geometry.height() - 1})
                for (int x : {0, geometry.width() - 1}) {
                    const auto p = geometry.source(x, y);
                    require(p[0] >= -0.5 - 1e-9 && p[0] <= 79.5 + 1e-9 && p[1] >= -0.5 - 1e-9 &&
                                p[1] <= 59.5 + 1e-9,
                            "Straighten canvas includes empty corners.");
                }
            const int x = geometry.width() / 2, y = geometry.height() / 2;
            const auto p = geometry.source(x, y);
            const auto sample = geometry.sample(x, y, pixel);
            require(std::abs(sample[0] - p[0]) < 1e-5 && std::abs(sample[1] - p[1]) < 1e-5 &&
                        sample[2] == -2,
                    "Bilinear affine/HDR sampling failed.");
        }
    luma::EditDocument tiny;
    tiny.crop.angle = 45;
    tiny.crop.width = 1. / 20000;
    tiny.crop.height = 1. / 20000;
    const luma::CropGeometry one(1, 20, 0, tiny);
    require(one.width() == 1 && one.height() == 1, "Tiny crop must retain one pixel.");
    require(one.sample(0, 0, [](int, int) { return std::array{3.F, -1.F, 0.5F}; }) ==
                std::array{3.F, -1.F, 0.5F},
            "Constant signed/HDR transform changed values.");
}
void validation() {
    luma::EditDocument edits;
    edits.setParameters({{"crop.x", 0.2},
                         {"crop.y", 0.1},
                         {"crop.width", 0.6},
                         {"crop.height", 0.7},
                         {"exposure", 0.5}});
    require(edits.crop.x == 0.2 && edits.crop.width == 0.6,
            "Atomic crop parameters depend on input order.");
    const auto before = edits;
    rejects([&] { edits.setParameters({{"crop.x", 0.7}, {"exposure", 1}}); });
    require(edits == before, "Invalid compound crop partially changed edits.");
    for (const auto *name :
         {"crop.x", "crop.y", "crop.width", "crop.height", "crop.angle", "crop.aspect",
          "crop.quarterTurns", "crop.flipHorizontal", "crop.flipVertical"})
        rejects([&] { edits.setParameter(name, std::numeric_limits<double>::quiet_NaN()); });
    for (const auto &entry :
         {std::pair{"crop.width", 0.}, std::pair{"crop.height", 2.},
          std::pair{"crop.quarterTurns", 0.5}, std::pair{"crop.flipHorizontal", 2.},
          std::pair{"crop.flipVertical", -1.}, std::pair{"crop.angle", 45.01},
          std::pair{"crop.aspect", -0.5}})
        rejects([&] { edits.setParameter(entry.first, entry.second); });
    rejects([&] { luma::CropGeometry geometry(8, 6, 8, edits); });
    const luma::CropGeometry geometry(8, 6, 0, edits);
    rejects([&] { geometry.source(-1, 0); });
    luma::FloatImage image(8, 6);
    luma::CancellationSource source;
    source.cancel();
    rejects([&] { luma::renderCPU(image, edits, 0, {}, source.token()); });
}
} // namespace
int main() {
    try {
        transforms();
        ratiosAndStraighten();
        validation();
        std::cout << "Crop exact transforms, ratios, straighten bounds, interpolation, validation "
                     "and cancellation passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
