#pragma once

#include "core/ai/Culling.h"
#include <memory>

namespace luma::macos {
std::shared_ptr<ai::ICullingProvider> makePhotoCulling();
} // namespace luma::macos
