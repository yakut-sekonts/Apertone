#pragma once
#include <filesystem>
#include <stop_token>
#include <string>

namespace luma::macos {
std::string sha256File(const std::filesystem::path &path, std::stop_token stop = {});
} // namespace luma::macos
