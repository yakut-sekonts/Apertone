#pragma once
#include <string>

namespace luma {
struct QuickLookLayer {
    std::string id;
    std::string recipe;
    double amount = 100;
    bool enabled = true;
    // Empty for a global look. Adaptive looks own a bitmap mask with this ID.
    std::string mask;
    bool operator==(const QuickLookLayer &) const = default;
    void validate() const;
};
} // namespace luma
