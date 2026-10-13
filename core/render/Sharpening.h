#pragma once
#include "core/image/FloatImage.h"
#include "core/jobs/Cancellation.h"
#include "core/render/DevelopPipeline.h"
#include <functional>

namespace luma {
enum class DetailDiagnostic { none, amount, radius, detail, masking };
class SharpeningPass {
  public:
    SharpeningPass(const FloatImage &input, const DevelopPipeline &pipeline,
                   const EditDocument &edits, CancellationToken stop = {}, double scale = 1,
                   DetailDiagnostic diagnostic = DetailDiagnostic::none,
                   const std::function<void(double)> &progress = {});
    std::array<float, 3> pixel(std::array<float, 3> developed, int x, int y) const;
    bool active() const {
        return !luminance_.empty();
    }

  private:
    int width_, height_;
    double amount_, detail_, masking_;
    DetailDiagnostic diagnostic_;
    std::vector<float> luminance_, blurred_;
    double mask(int x, int y) const;
    double residual(std::size_t index) const;
};
} // namespace luma
