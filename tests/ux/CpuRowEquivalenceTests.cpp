// Bounded instrumentation equivalence only; no UX race or color-accuracy claim.
#include "tests/ux/CpuRenderRowProbe.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
struct Observer final : luma::ux_test::CpuRowObserver {
    unsigned calls = 0;
    int width = 0, height = 0;
    bool valid = true;
    std::array<std::uint8_t, 64 * 4> row{};
    const luma::CancellationSource *cancelAfterRow = nullptr;
    void firstRowCompleted(const luma::DisplayImage &image) noexcept override {
        ++calls;
        width = image.width;
        height = image.height;
        if (width <= 0 || width > 64 || height < 2 ||
            image.rgba.size() < static_cast<std::size_t>(width) * 4) {
            valid = false;
            return;
        }
        // Read only the already computed first row, never the remaining buffer.
        std::copy_n(image.rgba.begin(), static_cast<std::size_t>(width) * 4, row.begin());
        if (cancelAfterRow)
            (void)cancelAfterRow->cancel();
    }
};
luma::DisplayImage instrumented(Observer &observer, const luma::FloatImage &image,
                                const luma::EditDocument &edits, int orientation = 0,
                                luma::MaskOverlay overlay = {}, luma::CancellationToken stop = {}) {
    return luma::renderCPUWithUxRows(observer, image, edits, orientation, {}, stop, {},
                                     luma::DetailDiagnostic::none, 1, {}, std::move(overlay));
}
void equivalent(const luma::FloatImage &image, const luma::EditDocument &edits, int orientation = 0,
                luma::MaskOverlay overlay = {}) {
    const auto beforeRGB = image.rgb, beforeAlpha = image.alpha;
    const auto ordinary = luma::renderCPU(image, edits, orientation, {}, {}, {},
                                          luma::DetailDiagnostic::none, 1, {}, overlay);
    if (!image.alpha.empty()) {
        bool fractional = false;
        for (int x = 0; x < ordinary.width; ++x) {
            const auto alpha = ordinary.rgba[static_cast<std::size_t>(x) * 4 + 3];
            fractional = fractional || (alpha > 0 && alpha < 255);
        }
        require(fractional, "Alpha fixture does not positively affect the observed first row");
    }
    Observer observer;
    const auto observed = instrumented(observer, image, edits, orientation, std::move(overlay));
    require(observer.valid && observer.calls == 1 && observer.width == ordinary.width &&
                observer.height == ordinary.height,
            "UX renderer did not observe exactly one real complete first row");
    require(observed.width == ordinary.width && observed.height == ordinary.height &&
                observed.rgba == ordinary.rgba,
            "UX instrumentation changed ordinary renderer output");
    require(std::equal(observer.row.begin(), observer.row.begin() + ordinary.width * 4,
                       ordinary.rgba.begin()),
            "Observed prefix is not the final independently rendered first row");
    require(image.rgb == beforeRGB && image.alpha == beforeAlpha,
            "UX instrumentation changed its immutable input");
}
} // namespace
int main() {
    try {
        luma::FloatImage source(64, 48);
        for (int y = 0; y < source.height; ++y)
            for (int x = 0; x < source.width; ++x)
                for (int channel = 0; channel < 3; ++channel)
                    source.rgb[(static_cast<std::size_t>(y) * source.width + x) * 3 + channel] =
                        static_cast<float>((x * 5 + y * 11 + channel * 29) % 151 + 1) / 160.F;
        equivalent(source, {});
        luma::EditDocument light;
        light.exposure = 1.25;
        light.contrast = 12;
        light.shadows = 17;
        equivalent(source, light);
        auto rotated = light;
        rotated.crop.quarterTurns = 1;
        rotated.crop.flipHorizontal = true;
        equivalent(source, rotated);
        auto alpha = source;
        alpha.alpha.resize(static_cast<std::size_t>(alpha.width) * alpha.height);
        for (std::size_t i = 0; i < alpha.alpha.size(); ++i)
            alpha.alpha[i] = static_cast<float>((i * 7) % 17) / 16.F;
        equivalent(alpha, light, 5);
        auto scaled = light;
        scaled.setEnabled(luma::Correction::transform, true);
        scaled.transform.scale = 80;
        scaled.transform.constrainCrop = false;
        equivalent(source, scaled);
        auto local = light;
        luma::LocalMask mask;
        mask.id = "00000000-0000-4000-8000-000000000001";
        mask.name = "Row equivalence";
        mask.shape = luma::MaskShape::radial;
        mask.x1 = 0.5;
        mask.y1 = 0.1;
        mask.x2 = 0.3;
        mask.y2 = 0.4;
        mask.adjustments[0] = 1;
        local.localMasks.push_back(mask);
        const luma::MaskOverlay overlay{mask.id, {0.2, 0.8, 0.4}, 0.6};
        const auto withoutOverlay = luma::renderCPU(source, local);
        const auto withOverlay = luma::renderCPU(source, local, 0, {}, {}, {},
                                                 luma::DetailDiagnostic::none, 1, {}, overlay);
        require(!std::equal(withOverlay.rgba.begin(), withOverlay.rgba.begin() + source.width * 4,
                            withoutOverlay.rgba.begin()),
                "Overlay fixture does not positively change the observed first row");
        equivalent(source, local, 0, overlay);

        luma::CancellationSource cancellation;
        Observer cancelledObserver;
        cancelledObserver.cancelAfterRow = &cancellation;
        std::string cancellationError;
        try {
            (void)instrumented(cancelledObserver, source, {}, 0, {}, cancellation.token());
        } catch (const std::exception &error) {
            cancellationError = error.what();
        }
        require(cancellationError == "Operation cancelled." && cancellation.isCancelled() &&
                    cancelledObserver.valid && cancelledObserver.calls == 1,
                "Original next-row cancellation did not stop the instrumented render");
        std::cout << "Six small renderer variants: exact output/prefix/input equivalence; "
                     "real next-row cancellation preserved\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
