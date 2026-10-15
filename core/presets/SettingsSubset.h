#pragma once
#include "core/edits/EditDocument.h"
#include <cstdint>
#include <span>
#include <string>

namespace luma {
enum class SettingsGroup : std::uint32_t {
    light = 1,
    color = 2,
    curve = 4,
    hsl = 8,
    detail = 16,
    crop = 32,
    optics = 64,
    geometry = 128
};
constexpr std::uint32_t allSettingsGroups = 255;
void validateSettingsGroups(std::uint32_t groups);
std::uint32_t settingsCorrections(std::uint32_t groups);
std::uint32_t settingsParameterGroup(std::string_view name);
std::uint32_t parameterSelectionGroups(std::span<const std::string> parameters);
EditDocument mergeSettings(const EditDocument &current, const EditDocument &source,
                           std::uint32_t groups, std::span<const std::string> parameters = {});
} // namespace luma
