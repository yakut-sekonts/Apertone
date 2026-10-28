#pragma once
#include "core/edits/EditDocument.h"
#include <array>
#include <span>
#include <string_view>
#include <vector>

namespace luma {
enum class QuickLookTarget { global, subject, background, sky };
struct QuickLookDelta {
    std::string_view parameter;
    double value;
};
struct QuickLookRecipe {
    std::string_view id, name, category, description;
    QuickLookTarget target = QuickLookTarget::global;
    std::vector<QuickLookDelta> deltas;
    std::array<double, 8> local{};
    double blur = 0;
};
std::span<const QuickLookRecipe> quickLookRecipes();
const QuickLookRecipe &quickLookRecipe(std::string_view id);
EditDocument resolveQuickLooks(const EditDocument &source);
std::array<double, 8> quickLookLocalAdjustments(const QuickLookRecipe &recipe, double amount);
double quickLookBlur(const QuickLookRecipe &recipe, double amount);
} // namespace luma
