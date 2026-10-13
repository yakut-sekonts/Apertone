#pragma once
#include "core/edits/EditDocument.h"
#include <cstdint>

namespace luma {
enum class SettingsGroup : std::uint32_t { light = 1, color = 2, curve = 4, hsl = 8, detail = 16 };
constexpr std::uint32_t allSettingsGroups = 31;
void validateSettingsGroups(std::uint32_t groups);
std::uint32_t settingsCorrections(std::uint32_t groups);
EditDocument mergeSettings(const EditDocument &current, const EditDocument &source,
                           std::uint32_t groups);
} // namespace luma
