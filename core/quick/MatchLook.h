#pragma once
#include "core/edits/EditDocument.h"
#include "core/edits/QuickLookTypes.h"
#include <span>
#include <vector>

namespace luma {
struct MatchLookOptions {
    bool includeExposure = false;
    bool includeWhiteBalance = false;
};

std::vector<QuickLookParameterDelta> matchLookDeltas(const EditDocument &target,
                                                     std::span<const EditDocument> references,
                                                     const MatchLookOptions &options = {});
bool isMatchLookParameter(std::string_view parameter,
                          const MatchLookOptions &options = {true, true});
} // namespace luma
