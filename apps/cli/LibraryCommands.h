#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace luma::cli {
bool libraryCommand(const std::filesystem::path &catalog, const std::vector<std::string> &args);
}
