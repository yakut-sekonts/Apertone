#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>

namespace luma {
inline constexpr std::array<std::string_view, 29> supportedRawExtensions{
    ".3fr", ".arw", ".cr2", ".cr3", ".crw", ".dcr", ".dcs", ".dng", ".erf", ".fff",
    ".gpr", ".iiq", ".k25", ".kdc", ".mef", ".mos", ".mrw", ".nef", ".nrw", ".orf",
    ".pef", ".ptx", ".raf", ".raw", ".rw2", ".rwl", ".sr2", ".srf", ".srw"};

inline std::string lowerPhotoExtension(const std::filesystem::path &path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) { return std::tolower(character); });
    return extension;
}

inline bool isSupportedRawExtension(const std::filesystem::path &path) {
    const auto extension = lowerPhotoExtension(path);
    return std::ranges::binary_search(supportedRawExtensions, extension);
}
} // namespace luma
