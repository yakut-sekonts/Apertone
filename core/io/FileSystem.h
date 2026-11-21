#pragma once

#include <cstdint>
#include <filesystem>

namespace luma::files {
int openRead(const std::filesystem::path &path, bool rejectLinks = false);
int createExclusive(const std::filesystem::path &path);
int flush(int descriptor);
int syncFile(const std::filesystem::path &path);
int syncDirectory(const std::filesystem::path &path);
int unlinkFile(const std::filesystem::path &path);
int move(const std::filesystem::path &source, const std::filesystem::path &target,
         bool replace = false);
// On Windows, moving the staging file installs it on volumes without hard links.
int publish(const std::filesystem::path &source, const std::filesystem::path &target);
std::int64_t processID();
bool processExited(std::int64_t owner);
} // namespace luma::files
