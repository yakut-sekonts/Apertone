#pragma once
#include "core/jobs/Cancellation.h"
#include "platform/windows/CameraPtpData.h"
#include <QByteArray>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace luma::windows {
struct TetherCamera {
    std::wstring id;
    std::wstring name;
};
struct TetherControlState {
    bool liveAvailable = false;
    bool live = false;
    std::vector<ptp::ExposureProperty> exposure;
};
struct TetherControlCommand {
    enum class Action { live, exposure, refresh, capture } action = Action::refresh;
    std::int64_t value = 0;
    std::uint16_t property = 0;
    std::filesystem::path destination;
};
class TetherControlRequests {
  public:
    void submit(TetherControlCommand request);
    std::optional<TetherControlCommand> next();

  private:
    std::mutex mutex_;
    std::deque<TetherControlCommand> queue_;
};
std::vector<TetherCamera> tetherCameras(CancellationToken stop = {});
std::vector<std::filesystem::path> captureTetheredPhotos(const std::wstring &id,
                                                         const std::filesystem::path &destination,
                                                         CancellationToken stop = {});
std::vector<std::filesystem::path>
controlTetheredCamera(const std::wstring &id, TetherControlRequests &requests,
                      const std::function<void(const TetherControlState &)> &state,
                      const std::function<void(const QByteArray &)> &liveFrame,
                      const std::function<void(const char *)> &warning, CancellationToken stop);
} // namespace luma::windows
