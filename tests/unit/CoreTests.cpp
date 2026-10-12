#include "apps/shared/SessionService.h"
#include "core/image/Bayer.h"
#include "core/jobs/JobManager.h"
#include "core/render/CpuRenderer.h"
#include <array>
#include <cmath>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <sqlite3.h>

namespace {
void require(bool result, const char *message) {
    if (!result) {
        throw std::runtime_error(message);
    }
}
template <typename F> void throws(F function) {
    bool caught = false;
    try {
        function();
    } catch (const std::exception &) {
        caught = true;
    }
    require(caught, "Expected an exception.");
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
        luma::cameraToLinearSRGB(image, {1, 0, 0, 0, 1, 0, 0, 0, 1});
        require(std::abs(image.rgb[0] - 0.8F) < 1e-6, "Identity camera matrix changed pixels.");
    }
    luma::FloatImage gray(2, 3);
    std::fill(gray.rgb.begin(), gray.rgb.end(), 0.18F);
    const auto raster = luma::renderCPU(gray, {});
    require(std::abs(static_cast<int>(raster.rgba[0]) - 118) <= 2,
            "Linear gray must use the sRGB transfer curve.");
    require(raster.rgba[3] == 255, "Display output must be opaque.");
    require(luma::renderCPU(gray, {1}).rgba[0] > raster.rgba[0],
            "Exposure did not change display pixels.");
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
    std::stop_source stop;
    stop.request_stop();
    throws([&] { luma::renderCPU(gray, {}, 0, {}, stop.get_token()); });
    throws([] { luma::FloatImage invalid(0, 10); });
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
        sqlite3 *db = nullptr;
        require(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "Cannot open schema test database.");
        require(sqlite3_exec(db, "PRAGMA user_version=2", nullptr, nullptr, nullptr) == SQLITE_OK,
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
    std::promise<void> firstStarted;
    std::promise<void> release;
    auto waiting = release.get_future().share();
    auto first = jobs.submit(luma::JobPriority::background, [&](const luma::JobContext &) {
        firstStarted.set_value();
        waiting.wait();
    });
    firstStarted.get_future().wait();
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
}
} // namespace
int main() {
    try {
        imageTests();
        catalogTests();
        jobTests();
        std::cout << "Core unit tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
