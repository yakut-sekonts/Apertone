#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace luma {
inline std::string pathUtf8(const std::filesystem::path &path) {
#ifdef _WIN32
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
#else
    return path.native();
#endif
}

inline std::filesystem::path pathFromUtf8(std::string_view text) {
#ifdef _WIN32
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
#else
    return std::filesystem::path(text);
#endif
}

inline std::filesystem::path pathWithSuffix(std::filesystem::path path, std::string_view suffix) {
    path += pathFromUtf8(suffix);
    return path;
}
} // namespace luma
