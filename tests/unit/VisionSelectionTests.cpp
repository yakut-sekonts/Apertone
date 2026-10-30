#include "core/render/CpuRenderer.h"
#include "platform/macos/RasterDecoder.h"
#include "platform/macos/VisionSegmentation.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Operation> void rejects(Operation operation) {
    try {
        operation();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error("Invalid Vision selection was accepted.");
}
} // namespace

int main() {
    try {
        const auto root = std::filesystem::path(APERTONE_SOURCE_DIR);
        const auto portrait =
            luma::macos::RasterDecoder().decode(root / "tests/assets/filters/portrait.png");
        const auto rendered = luma::renderCPU(portrait.image, {});
        const auto originalPixels = rendered.rgba;
        auto provider = luma::macos::makeVisionSegmentation();
        require(provider->location() == luma::ai::ExecutionLocation::local &&
                    provider->supports(luma::ai::Selection::subject) &&
                    provider->supports(luma::ai::Selection::face) &&
                    provider->supports(luma::ai::Selection::eyes) &&
                    provider->supports(luma::ai::Selection::lips) &&
                    !provider->supports(luma::ai::Selection::sky),
                "Vision provider advertised the wrong selection contract.");

        const luma::ai::RgbaImage input{rendered.width, rendered.height, rendered.rgba};
        const std::array<double, 2> facePoint{.43, .19};
        std::vector<std::size_t> selectedPixels;
        for (const auto selection :
             {luma::ai::Selection::face, luma::ai::Selection::eyes, luma::ai::Selection::lips}) {
            std::vector<double> progress;
            const auto mask = provider->segment(input, selection, facePoint, {},
                                                [&](double value) { progress.push_back(value); });
            const auto count = static_cast<std::size_t>(
                std::count_if(mask.pixels().begin(), mask.pixels().end(),
                              [](std::uint8_t value) { return value != 0; }));
            require(mask.width() == rendered.width && mask.height() == rendered.height &&
                        count > 20 && count < mask.pixels().size() / 3 && !progress.empty() &&
                        progress.back() == 1 && std::is_sorted(progress.begin(), progress.end()) &&
                        std::all_of(
                            progress.begin(), progress.end(),
                            [](double value) { return value > 0 && value <= 1; }),
                    "Vision people selection returned an invalid mask or progress sequence.");
            selectedPixels.push_back(count);
        }
        require(selectedPixels[0] > selectedPixels[1] && selectedPixels[0] > selectedPixels[2],
                "Detailed facial selections are not bounded by the face selection.");
        rejects([&] { provider->segment(input, luma::ai::Selection::face, {.99, .99}, {}, {}); });

        luma::CancellationSource cancelled;
        cancelled.cancel();
        rejects([&] {
            provider->segment(input, luma::ai::Selection::face, facePoint, cancelled.token(), {});
        });
        require(rendered.rgba == originalPixels,
                "Vision people selection mutated its immutable RGBA input.");
        std::cout
            << "Vision Face, Eyes and Lips selections passed on the licensed portrait fixture.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
