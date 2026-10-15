#pragma once
#include "core/edits/EditDocument.h"
#include "core/raw/RawDecoder.h"
namespace luma {
// A source-canvas sample before local layers, sharing the preview's global passes.
std::array<float, 3> sampleMaskReference(const FloatImage &source, const EditDocument &edits,
                                         const PhotoMetadata &metadata,
                                         RawColorCalibration calibration, double x, double y,
                                         double detailScale = 1, CancellationToken stop = {});
} // namespace luma
