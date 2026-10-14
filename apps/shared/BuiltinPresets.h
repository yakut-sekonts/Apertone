#pragma once
#include "apps/shared/PresetStore.h"

namespace luma {
// Stable IDs are independent of translated UI labels and user preset storage.
const std::vector<StoredPreset> &builtinPresets();
bool builtinPresetID(const QString &id);
} // namespace luma
