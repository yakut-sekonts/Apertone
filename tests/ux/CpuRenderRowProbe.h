#pragma once
#if !defined(LUMA_UX_CPU_RENDER_ROWS) && !defined(LUMA_UX_THUMBNAIL_TRACE)
#error "CPU row probe is test-target-only"
#endif
#include "core/render/CpuRenderer.h"

namespace luma::ux_test {
class CpuRowObserver {
  public:
    virtual ~CpuRowObserver() = default;
    // Called synchronously by the actual renderer after output row zero is complete.
    // The remaining image buffer must not be read by the observer.
    virtual void firstRowCompleted(const DisplayImage &image) noexcept = 0;
};
} // namespace luma::ux_test

namespace luma {
// The UX-only object compiles the same render body with this distinct symbol.
// No definition or new parameter is added to shipping renderCPU.
DisplayImage renderCPUWithUxRows(ux_test::CpuRowObserver &observer, const FloatImage &source,
                                 EditDocument edits, int orientation,
                                 std::span<const std::uint8_t> displayProfile,
                                 CancellationToken stop, RawColorCalibration calibration,
                                 DetailDiagnostic diagnostic, double detailScale,
                                 PhotoMetadata metadata, MaskOverlay overlay);
} // namespace luma
