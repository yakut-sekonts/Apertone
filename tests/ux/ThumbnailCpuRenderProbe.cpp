#include "tests/ux/ThumbnailCpuRenderProbe.h"
#include "tests/ux/CpuRenderRowProbe.h"
#include <utility>

namespace ux::thumbnail_trace {
namespace {
class FirstRowObserver final : public luma::ux_test::CpuRowObserver {
  public:
    FirstRowObserver(Token trace, std::uint64_t revision) noexcept
        : trace_(std::move(trace)), revision_(revision) {}
    void firstRowCompleted(const luma::DisplayImage &image) noexcept override {
        // Only the actual first converted row is copied. No uncomputed tail read.
        const auto rowBytes = static_cast<std::size_t>(image.width) * 4;
        if (image.width <= 0 || image.height <= 0 || image.rgba.size() < rowBytes) {
            trace_.afterActualCpuRow(revision_, image.width, image.height, {});
            return;
        }
        trace_.afterActualCpuRow(revision_, image.width, image.height,
                                 std::span(image.rgba).first(rowBytes));
    }

  private:
    Token trace_;
    std::uint64_t revision_;
};
} // namespace
luma::DisplayImage
renderThumbnailWithCpuProbe(const Token &trace, std::uint64_t revision,
                            const luma::FloatImage &source, luma::EditDocument edits,
                            int orientation, std::span<const std::uint8_t> displayProfile,
                            luma::CancellationToken stop, luma::RawColorCalibration calibration,
                            luma::DetailDiagnostic diagnostic, double detailScale,
                            luma::PhotoMetadata metadata, luma::MaskOverlay overlay) {
    if (!trace.requiresPartialCpuProbe())
        return luma::renderCPU(source, std::move(edits), orientation, displayProfile, stop,
                               calibration, diagnostic, detailScale, std::move(metadata), overlay);
    FirstRowObserver observer(trace, revision);
    trace.noteCpuStage(CpuStage::Entered, revision);
    try {
        auto image = luma::renderCPUWithUxRows(observer, source, std::move(edits), orientation,
                                               displayProfile, stop, calibration, diagnostic,
                                               detailScale, std::move(metadata), overlay);
        trace.noteCpuStage(CpuStage::Returned, revision, image.width, image.height);
        return image;
    } catch (...) {
        // Record the real exception path, including cancellation; never replace it.
        trace.noteCpuStage(CpuStage::Aborted, revision);
        throw;
    }
}
} // namespace ux::thumbnail_trace
