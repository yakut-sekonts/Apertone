#include "apps/shared/SessionService.h"
#include "core/color/DisplayTransform.h"
#include "core/color/DngColorCalibration.h"
#include "core/color/WhiteBalance.h"
#include "core/edits/EditHistory.h"
#include "core/export/JpegICC.h"
#include "core/image/Bayer.h"
#include "core/jobs/JobManager.h"
#include "core/render/CpuRenderer.h"
#include "core/render/DevelopPipeline.h"
#include "core/render/RetouchPass.h"
#include "tests/unit/LegacyCatalogFixture.h"
#include <array>
#include <cmath>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <source_location>
#include <sqlite3.h>

namespace {
void require(bool result, const char *message) {
    if (!result) {
        throw std::runtime_error(message);
    }
}
template <typename F>
void throws(F function, const std::source_location location = std::source_location::current()) {
    bool caught = false;
    try {
        function();
    } catch (const std::exception &) {
        caught = true;
    }
    if (!caught)
        throw std::runtime_error(
            "Expected an exception at CoreTests.cpp:" + std::to_string(location.line()) + ".");
}
void imageTests() {
    require(std::abs(luma::applyExposure(0.25F, 1) - 0.5F) < 1e-7,
            "Exposure must double linear light.");
    require(luma::applyExposure(2.0F, 2) == 8.0F, "HDR values must not clip before display.");
    require(luma::applyExposure(-0.1F, 1) < 0, "Negative color-transform values must survive.");
    throws([] { luma::applyExposure(1, std::numeric_limits<double>::quiet_NaN()); });
    throws([] { luma::applyExposure(1, 5.01); });
    for (const auto pattern : {std::array{0, 1, 1, 2}, std::array{2, 1, 1, 0},
                               std::array{1, 0, 2, 1}, std::array{1, 2, 0, 1}}) {
        luma::BayerFrame frame(6, 6, pattern);
        constexpr std::array<float, 3> expected{0.8F, 0.4F, 0.2F};
        for (int y = 0; y < 6; ++y) {
            for (int x = 0; x < 6; ++x) {
                frame.samples[y * 6 + x] = expected[frame.colorAt(x, y)];
            }
        }
        auto image = luma::demosaicBilinear(frame);
        for (std::size_t i = 0; i < image.rgb.size(); ++i) {
            require(std::abs(image.rgb[i] - expected[i % 3]) < 1e-6,
                    "CFA reconstruction failed, including edges.");
        }
        const auto edgeAware = luma::demosaicEdgeAware(frame);
        for (std::size_t i = 0; i < edgeAware.rgb.size(); ++i)
            require(std::abs(edgeAware.rgb[i] - expected[i % 3]) < 1e-6,
                    "Edge-aware CFA reconstruction changed a flat field.");
        luma::cameraToLinearSRGB(image, {1, 0, 0, 0, 1, 0, 0, 0, 1});
        require(std::abs(image.rgb[0] - 0.8F) < 1e-6, "Identity camera matrix changed pixels.");
    }
    luma::BayerFrame hardEdge(16, 12, {0, 1, 1, 2});
    for (int y = 0; y < hardEdge.height; ++y)
        for (int x = 0; x < hardEdge.width; ++x)
            hardEdge.samples[static_cast<std::size_t>(y) * hardEdge.width + x] = x < 8 ? .1F : 2.F;
    const auto bilinearEdge = luma::demosaicBilinear(hardEdge);
    const auto resolvedEdge = luma::demosaicEdgeAware(hardEdge);
    double bilinearError = 0, resolvedError = 0;
    for (int y = 0; y < hardEdge.height; ++y)
        for (int x = 0; x < hardEdge.width; ++x) {
            const float expected = x < 8 ? .1F : 2.F;
            const auto pixel = (static_cast<std::size_t>(y) * hardEdge.width + x) * 3;
            for (int color = 0; color < 3; ++color) {
                bilinearError += std::abs(bilinearEdge.rgb[pixel + color] - expected);
                resolvedError += std::abs(resolvedEdge.rgb[pixel + color] - expected);
            }
            require(resolvedEdge.rgb[pixel + hardEdge.colorAt(x, y)] == hardEdge.samples[pixel / 3],
                    "Edge-aware reconstruction changed an original sensor sample.");
        }
    require(resolvedError < 1e-5 && bilinearError > .5,
            "Edge-aware reconstruction did not suppress false color on a hard edge.");
    luma::BayerFrame chromaEdge(4, 4, {0, 1, 1, 2});
    for (int y = 0; y < chromaEdge.height; ++y)
        for (int x = 0; x < chromaEdge.width; ++x)
            chromaEdge.samples[static_cast<std::size_t>(y) * chromaEdge.width + x] =
                chromaEdge.colorAt(x, y) == 1 && x == 0 ? 1.F : 0.F;
    const auto boundedChroma = luma::demosaicEdgeAware(chromaEdge);
    require(std::all_of(boundedChroma.rgb.begin(), boundedChroma.rgb.end(),
                        [](float value) { return std::isfinite(value) && value >= 0; }),
            "Edge-aware reconstruction produced a negative radiometric sample.");
    luma::FloatImage recoverableHighlight(7, 7);
    for (std::size_t pixel = 0; pixel < 49; ++pixel) {
        recoverableHighlight.rgb[pixel * 3] = .8F;
        recoverableHighlight.rgb[pixel * 3 + 1] = .4F;
        recoverableHighlight.rgb[pixel * 3 + 2] = .2F;
    }
    const auto highlight = (std::size_t{3} * 7 + 3) * 3;
    // The raw sample can remain above the camera's linearity limit. Recovery
    // replaces that unreliable value; it is not merely a highlight boost.
    recoverableHighlight.rgb[highlight] = 1.8F;
    recoverableHighlight.rgb[highlight + 1] = .7F;
    recoverableHighlight.rgb[highlight + 2] = .35F;
    require(luma::recoverClippedHighlights(recoverableHighlight) == 1 &&
                std::abs(recoverableHighlight.rgb[highlight] - 1.4F) < 1e-6F &&
                recoverableHighlight.rgb[highlight + 1] == .7F &&
                recoverableHighlight.rgb[highlight + 2] == .35F,
            "Camera-linear highlight recovery did not restore local chromaticity.");
    auto unreliableAnchor = recoverableHighlight;
    unreliableAnchor.rgb[highlight] = 1.8F;
    unreliableAnchor.rgb[highlight + 1] = .99F;
    unreliableAnchor.rgb[highlight + 2] = .99F;
    const auto unreliableBefore = unreliableAnchor.rgb;
    require(luma::recoverClippedHighlights(unreliableAnchor) == 0 &&
                unreliableAnchor.rgb == unreliableBefore,
            "Highlight recovery used a near-clipped chromatic anchor.");
    luma::FloatImage fullyClipped(3, 3);
    std::fill(fullyClipped.rgb.begin(), fullyClipped.rgb.end(), .25F);
    fullyClipped.rgb[12] = 1.2F;
    fullyClipped.rgb[13] = 1.1F;
    fullyClipped.rgb[14] = 1.F;
    const auto fullyClippedBefore = fullyClipped.rgb;
    require(luma::recoverClippedHighlights(fullyClipped) == 0 &&
                fullyClipped.rgb == fullyClippedBefore,
            "Highlight recovery invented color for a fully clipped pixel.");
    fullyClipped.rgb[0] = std::numeric_limits<float>::quiet_NaN();
    throws([&] { luma::recoverClippedHighlights(fullyClipped); });
    luma::CancellationSource demosaicStop;
    demosaicStop.cancel();
    throws([&] { luma::demosaicEdgeAware(hardEdge, demosaicStop.token()); });
    throws([&] { luma::recoverClippedHighlights(recoverableHighlight, demosaicStop.token()); });
    hardEdge.samples[0] = std::numeric_limits<float>::quiet_NaN();
    throws([&] { luma::demosaicEdgeAware(hardEdge); });
    luma::FloatImage gray(2, 3);
    std::fill(gray.rgb.begin(), gray.rgb.end(), 0.18F);
    const auto raster = luma::renderCPU(gray, {});
    require(std::abs(static_cast<int>(raster.rgba[0]) - 118) <= 2,
            "Linear gray must use the sRGB transfer curve.");
    require(raster.rgba[3] == 255, "Display output must be opaque.");
    require(luma::renderCPU(gray, {1}).rgba[0] > raster.rgba[0],
            "Exposure did not change display pixels.");
    const std::array<float, 9> hdrDisplay{.18F, .18F, .18F, 1.F, 1.F, 1.F, 2.F, 2.F, 2.F};
    std::array<std::uint8_t, 12> clipped{}, mapped{};
    luma::DisplayTransform display;
    display.convert(hdrDisplay, clipped);
    display.convert(hdrDisplay, mapped, luma::DisplayMapping::adaptiveHDR);
    require(std::abs(int(mapped[0]) - int(clipped[0])) <= 1,
            "Adaptive HDR mapping changed mid-gray below its shoulder.");
    require(clipped[4] == 255 && clipped[8] == 255 && mapped[4] < mapped[8] && mapped[4] >= 235 &&
                mapped[8] <= 255,
            "Adaptive HDR mapping did not separate SDR white from HDR highlights.");
    luma::FloatImage sdrPreview(2, 1);
    sdrPreview.rgb = {.8F, .8F, .8F, .2F, .2F, .2F};
    const auto adaptivePreview = [](const luma::FloatImage &image) {
        return luma::renderCPU(image, {}, 0, {}, {}, {}, luma::DetailDiagnostic::none, 1, {}, {},
                               nullptr, {}, 4, luma::DisplayMapping::adaptiveHDR);
    };
    require(adaptivePreview(sdrPreview).rgba == luma::renderCPU(sdrPreview, {}).rgba,
            "Adaptive HDR mode changed an SDR-only preview.");
    luma::FloatImage hdrPreview(2, 1);
    hdrPreview.rgb = {1.F, 1.F, 1.F, 2.F, 2.F, 2.F};
    const auto adaptivePixels = adaptivePreview(hdrPreview).rgba;
    require(adaptivePixels[0] < adaptivePixels[4] && adaptivePixels[0] >= 235 &&
                adaptivePixels[4] <= 255,
            "HDR source detection did not activate preview tone mapping.");
    for (int orientation = 0; orientation < 8; ++orientation) {
        const auto image = luma::renderCPU(gray, {}, orientation);
        require(image.width == (orientation & 4 ? 3 : 2), "Rotated width is wrong.");
        require(image.height == (orientation & 4 ? 2 : 3), "Rotated height is wrong.");
    }
    gray.rgb[0] = 0.5F;
    const auto normal = luma::renderCPU(gray, {});
    const auto rotated = luma::renderCPU(gray, {}, 6);
    require(rotated.rgba[8] == normal.rgba[0], "90-degree orientation mapping is wrong.");
    const auto preview = luma::standardPreview(gray, 2);
    require(preview.width == 1 && preview.height == 2, "Preview bound is wrong.");
    luma::CancellationSource stop;
    stop.cancel();
    throws([&] { luma::renderCPU(gray, {}, 0, {}, stop.token()); });
    throws([] { luma::FloatImage invalid(0, 10); });
}
void retouchSourceTests() {
    for (const auto size : {std::array{64, 64}, std::array{97, 83}}) {
        luma::FloatImage image(size[0], size[1]);
        std::fill(image.rgb.begin(), image.rgb.end(), .25F);
        const auto center = (static_cast<std::size_t>(size[1] / 2) * size[0] + size[0] / 2) * 3;
        image.rgb[center] = 1;
        const auto original = image.rgb;
        luma::RetouchStroke stroke;
        stroke.id = "1234-abcd";
        stroke.mode = luma::RetouchMode::clone;
        stroke.radius = .06;
        stroke.points = {{.5, .5}};
        const auto offset = luma::findRetouchSource(image, stroke);
        require(offset == luma::findRetouchSource(image, stroke),
                "Retouch source search was not deterministic.");
        require(std::isfinite(offset[0]) && std::isfinite(offset[1]) &&
                    std::hypot(offset[0] * size[0], offset[1] * size[1]) >
                        2 * stroke.radius * std::min(size[0], size[1]),
                "Retouch source overlaps the target.");
        stroke.sourceOffsetX = offset[0];
        stroke.sourceOffsetY = offset[1];
        luma::RetouchPass repaired(image, {stroke});
        require(std::abs(repaired.image().rgb[center] - .25F) < 1e-6 && image.rgb == original,
                "Retouch did not repair the isolated defect or mutated its source.");
        require(luma::decodeRetouch(luma::encodeRetouch({stroke})) == std::vector{stroke},
                "Retouch persistence lost the resolved donor.");
        luma::CancellationSource cancelled;
        cancelled.cancel();
        throws([&] { luma::findRetouchSource(image, stroke, cancelled.token()); });
        image.alpha.assign(static_cast<std::size_t>(size[0]) * size[1], 0);
        throws([&] { luma::findRetouchSource(image, stroke); });
    }
}
void jpegProfileTests() {
    const std::vector<std::uint8_t> jpeg{255, 216, 255, 224, 0, 4, 1,   2,
                                         255, 218, 0,   2,   7, 8, 255, 217};
    const std::vector<std::uint8_t> profile(70000, 42);
    const auto tagged = luma::embedJpegICC(jpeg, profile);
    require(tagged[8] == 255 && tagged[9] == 226, "JPEG ICC marker missing.");
    require(tagged[24] == 1 && tagged[25] == 2, "Multipart JPEG ICC numbering is wrong.");
    require(std::equal(jpeg.begin(), jpeg.begin() + 8, tagged.begin()),
            "ICC embedding moved the JFIF header.");
    require(std::equal(jpeg.end() - 8, jpeg.end(), tagged.end() - 8),
            "ICC embedding changed compressed pixels.");
    require(luma::embedJpegICC(tagged, profile) == tagged,
            "ICC replacement accumulated duplicate segments.");
    throws([&] { luma::embedJpegICC({}, profile); });
    throws([&] { luma::embedJpegICC(jpeg, {}); });
    throws([&] {
        luma::embedJpegICC(std::vector<std::uint8_t>{255, 216, 255, 224, 255, 255}, profile);
    });
}
void developTests() {
    luma::EditDocument edits;
    luma::DevelopPipeline baseline(edits);
    const std::array<float, 3> pixel{0.5F, 0.25F, 0.125F};
    const auto neutral = baseline.pixel(pixel);
    for (std::size_t c = 0; c < 3; ++c)
        require(std::abs(neutral[c] - pixel[c]) < 1e-6, "Neutral Develop changed RGB.");
    edits.exposure = 1;
    const auto exposed = luma::DevelopPipeline(edits).pixel(pixel);
    require(std::abs(exposed[0] - 1) < 1e-6, "Exposure is not linear.");
    edits.setEnabled(luma::Correction::exposure, false);
    require(luma::DevelopPipeline(edits).pixel(pixel) == neutral,
            "Disabled Exposure changed output.");
    require(edits.exposure == 1, "Disabling erased a parameter.");
    edits = {};
    edits.saturation = -100;
    const auto gray = luma::DevelopPipeline(edits).pixel(pixel);
    require(gray[0] == gray[1] && gray[1] == gray[2], "Saturation -100 is not monochrome.");
    for (double temperature : {2000.0, 2850.0, 6500.0, 25000.0}) {
        edits = {};
        edits.customWhiteBalance = true;
        edits.temperature = temperature;
        edits.tint = 150;
        const auto result = luma::DevelopPipeline(edits).pixel(pixel);
        for (float channel : result)
            require(std::isfinite(channel), "WB boundary produced non-finite pixels.");
        edits.setEnabled(luma::Correction::whiteBalance, false);
        require(luma::DevelopPipeline(edits).pixel(pixel) == neutral,
                "Disabled manual WB must restore As Shot.");
    }
    for (double sign : {-100.0, 100.0}) {
        edits = {};
        edits.contrast = sign;
        edits.highlights = -sign;
        edits.shadows = sign;
        edits.whites = -sign;
        edits.blacks = sign;
        const luma::DevelopPipeline pipeline(edits);
        float previous = 0;
        for (int i = 0; i <= 1000; ++i) {
            const float value = static_cast<float>(std::exp2(-12 + i * 0.016));
            const auto result = pipeline.pixel({value, value, value});
            require(result[0] >= previous, "Tone boundary inverted luminance ordering.");
            previous = result[0];
        }
    }
    edits = {};
    edits.highlights = -100;
    require(luma::DevelopPipeline(edits).pixel({2, 2, 2})[0] < 2,
            "Highlights did not recover float headroom.");
    edits = {};
    edits.shadows = 100;
    require(luma::DevelopPipeline(edits).pixel({0.02F, 0.02F, 0.02F})[0] > 0.02F,
            "Shadows did not lift low light.");
    edits = {};
    edits.vibrance = 100;
    require(luma::DevelopPipeline(edits).pixel({0.2F, 0.2F, 0.2F})[0] == 0.2F,
            "Vibrance altered neutral gray.");
    throws([&] { edits.setParameter("tint", 151); });
    throws([&] { edits.setParameter("exposure", std::numeric_limits<double>::quiet_NaN()); });
    throws([&] { edits.setParameter("unknown", 1); });
    require(edits.tint == 0, "Invalid edit partially mutated document.");
    luma::EditHistory history;
    history.begin();
    for (int i = 1; i <= 20; ++i) {
        auto next = history.current();
        next.exposure = i / 10.0;
        history.apply(next);
    }
    history.end();
    require(history.canUndo(), "Gesture has no undo.");
    history.undo();
    require(history.current() == luma::EditDocument{} && !history.canUndo(),
            "One slider gesture must be one Undo.");
    history.redo();
    require(history.current().exposure == 2, "Redo failed.");
    history.undo();
    edits = history.current();
    edits.shadows = 30;
    history.apply(edits);
    require(!history.canRedo(), "New edit did not invalidate redo.");
    history.apply({});
    history.undo();
    require(history.current().shadows == 30, "Reset must be one reversible operation.");

    luma::EditHistory provisional;
    provisional.begin();
    auto dragged = provisional.current();
    dragged.exposure = 1.5;
    provisional.apply(dragged);
    auto restoredGesture = luma::EditHistory(provisional.persisted());
    require(restoredGesture.current() == dragged && restoredGesture.canUndo(),
            "A provisional autosave was not restored as one history step.");
    restoredGesture.undo();
    require(restoredGesture.current() == luma::EditDocument{},
            "Restored provisional history did not retain its gesture base.");
}
void dngCalibrationTests() {
    luma::RawColorCalibration calibration;
    calibration.dngProfileCount = 2;
    calibration.analogBalance = {1.03F, .98F, 1.01F};
    calibration.dngProfiles[0].calibrationTemperature = 2856;
    calibration.dngProfiles[0].colorMatrix = {1.1F,  -.05F, -.05F, -.05F, 1.1F,
                                              -.05F, -.02F, -.08F, 1.1F};
    calibration.dngProfiles[0].forwardMatrix = {.82F, .1442F, 0, .08F, .84F, .08F, 0, .16F, .6649F};
    calibration.dngProfiles[0].cameraCalibration = {1.01F, .01F, 0, 0, .99F, .01F, 0, .01F, 1.02F};
    calibration.dngProfiles[0].hasForwardMatrix = true;
    calibration.dngProfiles[1].calibrationTemperature = 6504;
    calibration.dngProfiles[1].colorMatrix = {.9F, .08F, .02F, .03F, .94F, .03F, .02F, .1F, .88F};
    calibration.dngProfiles[1].forwardMatrix = {.74F, .2242F, 0,   .12F,  .78F,
                                                .1F,  .02F,   .2F, .6049F};
    calibration.dngProfiles[1].cameraCalibration = {.99F, .01F, 0, .01F, 1.01F, 0, 0, .02F, .98F};
    calibration.dngProfiles[1].hasForwardMatrix = true;
    calibration.asShotNeutral = {.8F, 1, 1.35F};
    luma::finalizeDngColorCalibration(calibration);

    const auto warm = luma::resolveRawColorCalibration(calibration, 2856, 0);
    const auto daylight = luma::resolveRawColorCalibration(calibration, 6504, 0);
    require(warm.cameraToSRGB != daylight.cameraToSRGB,
            "Dual-illuminant DNG matrix did not follow White Balance.");
    require(warm.gains != daylight.gains,
            "Dual-illuminant DNG camera neutral did not follow White Balance.");
    const auto warmWhite = luma::whitePointXYZ(2856, 0);
    std::array<double, 3> referenceNeutral{}, warmNeutral{};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 3; ++column)
            referenceNeutral[row] +=
                calibration.dngProfiles[0].colorMatrix[row * 3 + column] * warmWhite[column];
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 3; ++column)
            warmNeutral[row] += calibration.analogBalance[row] *
                                calibration.dngProfiles[0].cameraCalibration[row * 3 + column] *
                                referenceNeutral[column];
    const auto estimated = luma::estimateWhiteBalance(warmNeutral, calibration);
    require(std::abs(estimated.temperature - 2856) < 120 && std::abs(estimated.tint) < 3,
            "DNG WB estimation ignored the WB-dependent color matrix.");
    const double midpointMired = (1'000'000 / 2856.0 + 1'000'000 / 6504.0) / 2;
    const auto midpoint =
        luma::resolveRawColorCalibration(calibration, 1'000'000 / midpointMired, 0);
    require(midpoint.cameraToSRGB != warm.cameraToSRGB &&
                midpoint.cameraToSRGB != daylight.cameraToSRGB,
            "DNG reciprocal-temperature midpoint collapsed to an endpoint profile.");
    for (float value : midpoint.cameraToSRGB)
        require(std::isfinite(value), "DNG interpolation produced a non-finite matrix.");

    luma::EditDocument custom;
    custom.customWhiteBalance = true;
    custom.temperature = 2856;
    const luma::DevelopPipeline pipeline(custom, calibration);
    const auto renderedNeutral =
        pipeline.pixel({static_cast<float>(warmNeutral[0]), static_cast<float>(warmNeutral[1]),
                        static_cast<float>(warmNeutral[2])});
    require(std::abs(renderedNeutral[0] - renderedNeutral[1]) < 2e-5 &&
                std::abs(renderedNeutral[1] - renderedNeutral[2]) < 2e-5,
            "DNG ForwardMatrix path did not map the selected camera neutral to D50.");
    const auto gpu = pipeline.gpuParameters();
    for (std::size_t i = 0; i < 9; ++i)
        require(std::abs(gpu[i] - warm.cameraToSRGB[i]) < 1e-6,
                "CPU/Metal parameters do not share the resolved DNG matrix.");
    for (std::size_t i = 0; i < 3; ++i)
        require(std::abs(gpu[9 + i] - warm.gains[i]) < 1e-6,
                "CPU/Metal parameters do not share the resolved DNG gains.");

    luma::DevelopPipeline asShot({}, calibration);
    const auto asShotGpu = asShot.gpuParameters();
    for (std::size_t i = 0; i < 9; ++i)
        require(std::abs(asShotGpu[i] - calibration.cameraToSRGB[i]) < 1e-6,
                "As Shot rendering discarded the finalized DNG matrix.");
    require(std::abs(calibration.asShotGains[0] - 1.25F) < 1e-6 &&
                std::abs(calibration.asShotGains[2] - 1.F / 1.35F) < 1e-6,
            "DNG AsShotNeutral was not converted to channel gains.");
    const auto renderedAsShot = asShot.pixel(calibration.asShotNeutral);
    require(std::abs(renderedAsShot[0] - renderedAsShot[1]) < 2e-5 &&
                std::abs(renderedAsShot[1] - renderedAsShot[2]) < 2e-5,
            "DNG As Shot matrix was not factored with the exact AsShotNeutral gains.");

    auto fallback = calibration;
    fallback.dngProfiles[0].hasForwardMatrix = false;
    fallback.dngProfiles[1].hasForwardMatrix = false;
    luma::finalizeDngColorCalibration(fallback);
    const auto fallbackNeutral =
        luma::DevelopPipeline(custom, fallback)
            .pixel({static_cast<float>(warmNeutral[0]), static_cast<float>(warmNeutral[1]),
                    static_cast<float>(warmNeutral[2])});
    require(std::abs(fallbackNeutral[0] - fallbackNeutral[1]) < 2e-5 &&
                std::abs(fallbackNeutral[1] - fallbackNeutral[2]) < 2e-5,
            "DNG ColorMatrix Bradford fallback did not preserve the selected neutral.");

    auto invalid = calibration;
    invalid.dngProfiles[0].colorMatrix = {};
    throws([&] { luma::finalizeDngColorCalibration(invalid); });
    require(luma::dngIlluminantTemperature(17) == 2856 && !luma::dngIlluminantTemperature(255),
            "DNG calibration illuminant validation is wrong.");
}
void catalogTests() {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("luma-unit-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    try {
        const auto source = directory / "Кадр с пробелами.NEF";
        std::ofstream(source) << "synthetic test bytes";
        const auto path = directory / "catalog.sqlite";
        std::string id;
        {
            luma::Catalog catalog(path);
            require(!catalog.activeAsset(), "New catalog should be empty.");
            luma::PhotoMetadata metadata;
            metadata.width = 6;
            metadata.height = 4;
            metadata.camera = "Synthetic";
            const auto asset = catalog.addAndSelect(source, "test-checksum", metadata);
            id = asset.id;
            require(id.size() == 36, "Asset ID must be a UUID.");
            catalog.saveExposure(id, 1.25);
            require(catalog.addAndSelect(source, "test-checksum", metadata).edits.exposure == 1.25,
                    "Reimport lost edits.");
            throws([&] { catalog.addAndSelect(source, "changed-checksum", metadata); });
            throws([&] { catalog.saveExposure(id, std::numeric_limits<double>::infinity()); });
            throws([&] { catalog.saveExposure("missing-id", 0); });
            catalog.verify();
        }
        {
            luma::Catalog reopened(path);
            const auto asset = reopened.activeAsset();
            require(asset && asset->id == id && asset->edits.exposure == 1.25,
                    "Durable edit or selected photo was lost after restart.");
            require(asset->path.filename() == source.filename(), "Unicode path was corrupted.");
            reopened.verify();
        }
        {
            luma::Catalog catalog(path);
            auto edits = catalog.activeAsset()->edits;
            edits.temperature = 5350;
            edits.tint = 12;
            edits.customWhiteBalance = true;
            edits.highlights = -45;
            edits.shadows = 23;
            edits.vibrance = 35;
            edits.setEnabled(luma::Correction::shadows, false);
            catalog.saveEdits(id, edits);
            require(catalog.activeAsset()->edits == edits, "Develop document round trip failed.");
            catalog.saveExposure(id, 0.75);
            require(catalog.activeAsset()->edits.temperature == 5350,
                    "Legacy Exposure update erased Develop.");
        }
        {
            luma::Catalog catalog(path);
            luma::EditHistory history(catalog.activeAsset()->edits);
            auto first = history.current();
            first.exposure = -1.25;
            history.apply(first, "Exposure pass");
            auto second = history.current();
            second.contrast = 30;
            history.apply(second, "Contrast pass");
            history.undo();
            const auto persisted = history.persisted();
            catalog.saveEditsAndHistory(id, history.current(), persisted);
            require(catalog.editHistory(id).has_value(),
                    "Catalog did not store per-photo processing history.");
            auto mismatch = persisted;
            mismatch.entries[mismatch.currentIndex].document.exposure = 2;
            throws([&] { catalog.saveEditsAndHistory(id, history.current(), mismatch); });
            require(catalog.activeAsset()->edits == history.current(),
                    "Rejected history mismatch changed the saved edit.");

            sqlite3 *failure = nullptr;
            require(sqlite3_open(path.c_str(), &failure) == SQLITE_OK &&
                        sqlite3_exec(failure,
                                     "CREATE TRIGGER fail_history BEFORE INSERT ON "
                                     "edit_history_states BEGIN SELECT "
                                     "RAISE(ABORT,'injected history failure');END;",
                                     nullptr, nullptr, nullptr) == SQLITE_OK,
                    "Cannot install processing-history failure fixture.");
            sqlite3_close(failure);
            luma::EditHistory failed(persisted);
            failed.redo();
            throws([&] { catalog.saveEditsAndHistory(id, failed.current(), failed.persisted()); });
            require(catalog.activeAsset()->edits == history.current() &&
                        catalog.editHistory(id)->currentIndex == persisted.currentIndex,
                    "Failed history write partially committed its edit or cursor.");
            require(sqlite3_open(path.c_str(), &failure) == SQLITE_OK &&
                        sqlite3_exec(failure, "DROP TRIGGER fail_history", nullptr, nullptr,
                                     nullptr) == SQLITE_OK,
                    "Cannot remove processing-history failure fixture.");
            sqlite3_close(failure);
        }
        {
            luma::Catalog catalog(path);
            const auto persisted = catalog.editHistory(id);
            require(persisted.has_value(), "Restart lost per-photo processing history.");
            luma::EditHistory restored(*persisted);
            require(restored.current().exposure == -1.25 && restored.current().contrast == 0 &&
                        restored.canUndo() && restored.canRedo(),
                    "Restart did not preserve the history cursor and Redo branch.");
            restored.redo();
            require(restored.current().contrast == 30,
                    "Restarted processing history could not replay Redo.");
            catalog.saveExposure(id, 0.75);
            require(!catalog.editHistory(id),
                    "Legacy edit write did not clear its stale processing history.");
        }
        sqlite3 *db = nullptr;
        require(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "Cannot open schema test database.");
        require(sqlite3_exec(db, fixture::removePostV11Tables, nullptr, nullptr, nullptr) ==
                    SQLITE_OK,
                "Cannot remove newer features from the historical M0 fixture.");
        const char *legacy = R"sql(
            BEGIN;
            CREATE TABLE legacy_edits(asset_id TEXT PRIMARY KEY REFERENCES assets(id) ON DELETE CASCADE,
                schema_version INTEGER NOT NULL DEFAULT 1 CHECK(schema_version=1),
                exposure REAL NOT NULL DEFAULT 0 CHECK(exposure BETWEEN -5 AND 5),
                revision INTEGER NOT NULL DEFAULT 0,updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
            INSERT INTO legacy_edits(asset_id,exposure,revision,updated_at)
                SELECT asset_id,exposure,revision,updated_at FROM edit_documents;
            DROP TRIGGER initialize_local_masks; DROP TABLE edit_local_masks;
            DROP TABLE edit_documents;
            ALTER TABLE legacy_edits RENAME TO edit_documents;
            DROP TABLE edit_curve_points;
            DROP TABLE edit_hsl_bands;
            DROP TABLE import_items;
            DROP TABLE import_sessions;
            DROP TABLE collection_assets;
            DROP TABLE collections;
            DROP INDEX assets_filename;
            DROP INDEX assets_flag;
            DELETE FROM schema_migrations WHERE version>=2;
            PRAGMA user_version=1;
            COMMIT;
        )sql";
        require(sqlite3_exec(db, legacy, nullptr, nullptr, nullptr) == SQLITE_OK,
                "Cannot establish historical M0 fixture.");
        sqlite3_close(db);
        {
            luma::Catalog migrated(path);
            const auto asset = migrated.activeAsset();
            require(asset && asset->id == id && asset->edits.exposure == 0.75,
                    "M0 migration lost exposure or selection.");
            require(!asset->edits.customWhiteBalance && asset->edits.temperature == 6500,
                    "Migration defaults changed As Shot.");
            migrated.verify();
        }
        require(sqlite3_open(path.c_str(), &db) == SQLITE_OK,
                "Cannot reopen future schema fixture.");
        require(sqlite3_exec(db, "PRAGMA user_version=24", nullptr, nullptr, nullptr) == SQLITE_OK,
                "Cannot create future catalog.");
        sqlite3_close(db);
        throws([&] { luma::Catalog future(path); });
        std::filesystem::remove_all(directory);
    } catch (...) {
        std::filesystem::remove_all(directory);
        throw;
    }
}
void jobTests() {
    luma::JobManager jobs;
    require(jobs.idle(), "An empty job pool is not idle.");
    std::promise<void> firstStarted;
    std::promise<void> release;
    auto waiting = release.get_future().share();
    auto first = jobs.submit(luma::JobPriority::background, [&](const luma::JobContext &) {
        firstStarted.set_value();
        waiting.wait();
    });
    firstStarted.get_future().wait();
    require(!jobs.idle(), "Running worker was reported idle.");
    std::atomic<bool> executed = false;
    auto cancelled = jobs.submit(luma::JobPriority::background,
                                 [&](const luma::JobContext &) { executed = true; });
    cancelled.cancel();
    std::promise<void> finished;
    auto interactive =
        jobs.submit(luma::JobPriority::interactive, [&](const luma::JobContext &context) {
            context.setProgress(0.5);
            finished.set_value();
        });
    release.set_value();
    require(finished.get_future().wait_for(std::chrono::seconds(3)) == std::future_status::ready,
            "Interactive job stalled.");
    jobs.shutdown();
    require(!executed, "Cancelled job ran.");
    require(cancelled.state() == luma::JobState::cancelled,
            "Cancelled job state was not retained.");
    require(first.state() == luma::JobState::completed, "Completed job state is wrong.");
    require(interactive.progress() == 1, "Job progress did not complete.");
    require(jobs.idle(), "A stopped, drained pool is not idle.");
}
void cancelledWorkerIdleTests() {
    luma::JobManager jobs;
    std::promise<void> started, release;
    auto waiting = release.get_future().share();
    auto previous = jobs.submit(luma::JobPriority::background, [&](const luma::JobContext &) {
        started.set_value();
        waiting.wait();
    });
    started.get_future().wait();
    previous.cancel();
    auto newest = jobs.submit(luma::JobPriority::background, [](const luma::JobContext &) {});
    newest.cancel();
    jobs.submit(luma::JobPriority::interactive, [](const luma::JobContext &) {});
    const bool newestTerminal = newest.state() == luma::JobState::cancelled;
    const bool oldWorkerRetained = previous.state() == luma::JobState::running && !jobs.idle();
    release.set_value();
    jobs.shutdown();
    require(newestTerminal, "Cancelled queued handle did not reach its terminal state.");
    require(oldWorkerRetained, "Terminal replacement handle hid an older cancelled worker.");
    require(jobs.idle() && previous.state() == luma::JobState::cancelled,
            "Cancelled worker was not drained before idle.");
}
} // namespace
int main() {
    try {
        imageTests();
        retouchSourceTests();
        developTests();
        dngCalibrationTests();
        jpegProfileTests();
        catalogTests();
        jobTests();
        cancelledWorkerIdleTests();
        std::cout << "Core unit tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
