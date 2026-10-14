#pragma once
#include "core/edits/EditDocument.h"
#include "core/export/ExportOptions.h"
#include "core/raw/RawDecoder.h"

namespace luma {
// Straight-alpha, scene-linear sRGB. Signed values and HDR survive until encoding.
struct LinearOutput {
    int width, height;
    std::vector<float> rgba;
    LinearOutput(int width, int height);
    void validate() const;
};
LinearOutput renderLinearOutput(const DecodedRaw &decoded, const EditDocument &edits,
                                CancellationToken stop = {},
                                const std::function<void(double)> &progress = {});
LinearOutput resizeOutput(LinearOutput input, const ExportOptions &options,
                          CancellationToken stop = {});
void sharpenOutput(LinearOutput &image, const ExportOptions &options, CancellationToken stop = {});
} // namespace luma
