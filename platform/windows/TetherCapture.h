#pragma once
#include "core/jobs/Cancellation.h"
#include <filesystem>
#include <string>
#include <vector>

namespace luma::windows {
struct TetherCamera {
    std::wstring id;
    std::wstring name;
};
std::vector<TetherCamera> tetherCameras(CancellationToken stop = {});
std::vector<std::filesystem::path> captureTetheredPhotos(const std::wstring &id,
                                                         const std::filesystem::path &destination,
                                                         CancellationToken stop = {});
} // namespace luma::windows
