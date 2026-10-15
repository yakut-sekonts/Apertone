#pragma once
#include "core/edits/LocalMasks.h"
namespace luma {
std::array<double, 3> maskReferenceColor(std::array<float, 3> linear);
double maskReferenceLuminance(std::array<float, 3> linear);
class PrimitiveCoverage {
  public:
    explicit PrimitiveCoverage(MaskGeometry geometry, double aspect = 1);
    double coverage(double x, double y, std::array<float, 3> reference) const;

  private:
    struct Segment {
        std::array<double, 2> a, b;
        double radius;
        std::size_t stroke;
    };
    struct Node {
        std::array<double, 4> bounds{};
        std::size_t first = 0, count = 0;
        int left = -1, right = -1;
    };
    MaskGeometry geometry_;
    double xScale_ = 1, yScale_ = 1;
    std::vector<Segment> segments_;
    std::vector<Node> nodes_;
    int build(std::size_t first, std::size_t count);
    void query(int node, double x, double y, std::array<double, 256> &amounts) const;
};
class MaskCoverage {
  public:
    explicit MaskCoverage(const LocalMask &mask, double aspect = 1);
    double coverage(double x, double y, std::array<float, 3> reference = {}) const;

  private:
    struct Operand {
        MaskOperation operation;
        bool inverted;
        PrimitiveCoverage primitive;
    };
    PrimitiveCoverage base_;
    std::vector<Operand> operands_;
    bool enabled_, inverted_;
    double opacity_;
};
} // namespace luma
