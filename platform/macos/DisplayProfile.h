#pragma once
#include <cstdint>
#include <vector>

namespace luma::macos {
std::vector<std::uint8_t> displayProfile(std::uintptr_t nativeView);
} // namespace luma::macos
