#pragma once
#include "core/metadata/DescriptiveMetadata.h"
#include <span>
#ifdef __APPLE__
#include "platform/macos/CFHandle.h"
#include <ImageIO/ImageIO.h>
#endif

namespace luma::macos {
DescriptiveMetadata readDescriptiveMetadata(const std::filesystem::path &source,
                                            std::span<const std::uint8_t> sidecar = {});
// Rewrites only Apertone's descriptive properties; other parsed XMP properties are retained.
std::vector<std::uint8_t> writeDescriptiveXMP(const DescriptiveMetadata &values,
                                              std::span<const std::uint8_t> existing = {});
#ifdef __APPLE__
CFHandle<CGMutableImageMetadataRef> applyMetadataOverlay(CGImageMetadataRef source,
                                                         const MetadataOverlay &overlay);
#endif
} // namespace luma::macos
