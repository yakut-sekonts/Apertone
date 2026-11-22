#pragma once

#include "apps/shared/PresetStore.h"
#include "core/library/SmartCollection.h"
#include <map>
#include <string_view>

namespace luma {
// Reads the saved literal table used by Classic catalogs and projects it through
// the same mapping as XMP presets. The table is data and is never evaluated.
using LightroomProperties = std::map<std::string, QString, std::less<>>;
struct LightroomMaskGroup {
    LightroomProperties fields;
    std::vector<LightroomProperties> components;
};
struct LightroomDevelopExtras {
    QString cameraProfile;
    std::vector<LightroomMaskGroup> masks;
};
LightroomDevelopExtras readLightroomDevelopExtras(std::string_view document);
PresetImport decodeLightroomDevelop(std::string_view document,
                                    LightroomDevelopExtras *extras = nullptr);
SmartRule decodeLightroomSmartRule(std::string_view document);
} // namespace luma
