#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace luma {
inline constexpr std::size_t maxQuickLookParameterDeltas = 64;
struct QuickLookParameterDelta {
    std::string parameter;
    double value = 0;
    bool operator==(const QuickLookParameterDelta &) const = default;
};
struct QuickLookLayer {
    std::string id;
    std::string recipe;
    double amount = 100;
    bool enabled = true;
    // Empty for a global look. Adaptive looks own a bitmap mask with this ID.
    std::string mask;
    // Match Look layers are self-contained so rendering and export do not depend
    // on a mutable user preset collection. Built-in layers leave both fields empty.
    std::string name;
    std::vector<QuickLookParameterDelta> deltas;
    bool operator==(const QuickLookLayer &) const = default;
    void validate() const;
};
} // namespace luma
