#pragma once

#include "apps/shared/PresetStore.h"
#include <string_view>

namespace luma {
// Reads the saved literal table used by Classic catalogs and projects it through
// the same mapping as XMP presets. The table is data and is never evaluated.
PresetImport decodeLightroomDevelop(std::string_view document);
} // namespace luma
