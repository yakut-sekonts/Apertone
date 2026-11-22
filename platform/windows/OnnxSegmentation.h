#pragma once
#include "core/ai/Providers.h"
#include <memory>

namespace luma::windows {
std::shared_ptr<ai::ISegmentationProvider> makeOnnxSegmentation();
}
