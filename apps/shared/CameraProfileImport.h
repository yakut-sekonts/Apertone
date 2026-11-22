#pragma once
#include "core/color/DcpProfile.h"
#include "core/jobs/Cancellation.h"
#include <QString>
#include <filesystem>
#include <map>

namespace luma {
DcpSelection loadCameraProfile(const std::filesystem::path &source);
bool cameraProfileMatches(const DcpProfile &profile, const std::string &camera);

class CameraProfileDirectory {
  public:
    CameraProfileDirectory(const std::filesystem::path &folder, CancellationToken stop);
    DcpSelection find(const QString &name, const std::string &camera, CancellationToken stop);

  private:
    std::vector<std::filesystem::path> files_;
    std::map<std::pair<QString, QString>, DcpSelection> resolved_;
};
} // namespace luma
