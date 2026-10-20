#pragma once
#if !defined(LUMA_UX_THUMBNAIL_TRACE)
#error "Thumbnail CPU render probe is test-target-only"
#endif
#include "core/render/CpuRenderer.h"
#include "tests/ux/ThumbnailResponseTrace.h"

namespace ux::thumbnail_trace {
// Provider-only entry. Existing sessions/default gates call original renderCPU.
luma::DisplayImage
renderThumbnailWithCpuProbe(const Token &trace, std::uint64_t revision,
                            const luma::FloatImage &source, luma::EditDocument edits,
                            int orientation, std::span<const std::uint8_t> displayProfile,
                            luma::CancellationToken stop, luma::RawColorCalibration calibration,
                            luma::DetailDiagnostic diagnostic, double detailScale,
                            luma::PhotoMetadata metadata, luma::MaskOverlay overlay = {});
} // namespace ux::thumbnail_trace
