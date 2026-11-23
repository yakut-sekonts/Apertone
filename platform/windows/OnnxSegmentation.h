#pragma once
#include "core/ai/Providers.h"
#include <filesystem>
#include <memory>

namespace luma::windows {
std::shared_ptr<ai::ISegmentationProvider> makeOnnxSegmentation();
std::shared_ptr<ai::IEmbeddingProvider> makeOnnxCategoryEmbedding();
std::shared_ptr<ai::IDenoiseProvider> makeOnnxDenoise(const std::filesystem::path &model);
} // namespace luma::windows
