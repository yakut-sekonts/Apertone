#include "core/library/LibraryTypes.h"
#include <stdexcept>

namespace luma {
void validateLabel(const std::string &label) {
    if (!label.empty() && label != "red" && label != "yellow" && label != "green" &&
        label != "blue" && label != "purple")
        throw std::runtime_error("Unknown color label.");
}
void LibraryFilter::validate() const {
    if (minimumRating < 0 || minimumRating > 5 || flag < -2 || flag > 1 ||
        cullingRecommendation < -2 || cullingRecommendation > 1 || faceGroupID.size() > 64)
        throw std::runtime_error("Invalid library filter.");
    if (label)
        validateLabel(*label);
    if (search.size() > 1024)
        throw std::runtime_error("Search is too long.");
}
} // namespace luma
