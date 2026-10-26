#pragma once
#include <QRectF>
#include <QString>
#include <memory>

class QQuickWindow;

namespace ux {
// Public AppKit handoff. Optional cooperative peer uses this test binary only.
// No OS setting, CGEvent, controller mutation or synthetic activation notification.
struct NativeHandoffState {
    bool applicationActive = false, applicationHidden = false;
    bool ownRunningActive = false, peerActive = false, peerTerminated = false;
    bool ownFrontmost = false, peerFrontmost = false;
    bool windowKey = false, windowVisible = false, windowMiniaturized = false;
    int ownPID = 0, peerPID = 0, windowNumber = 0, activationPolicy = 0;
    int peerActivationPolicy = 0;
    unsigned appResigned = 0, appActivated = 0, windowResignedKey = 0;
    unsigned peerActivated = 0, ownActivated = 0;
};

// Passive identity/geometry for external native UI targeting. Coordinates are
// AppKit global points, not CUA screen pixels; no input or activation is performed.
struct NativeHandoffIdentity {
    int ownPID = 0, windowNumber = 0;
    QString executablePath, bundleIdentifier, bundlePath, localizedName, windowTitle;
    QRectF windowFrameAppKit, screenFrameAppKit;
};

class NativeApplicationHandoff final {
  public:
    explicit NativeApplicationHandoff(QQuickWindow &window, const QString &peerRequestPath = {});
    ~NativeApplicationHandoff();
    NativeApplicationHandoff(const NativeApplicationHandoff &) = delete;
    NativeApplicationHandoff &operator=(const NativeApplicationHandoff &) = delete;
    NativeHandoffState state() const;
    NativeHandoffIdentity identity() const;
    // Dispatch result is never activation proof; independently observe fresh native/Qt state.
    bool requestPeerActivation();
    void requestOwnActivation();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Internal owned-child entry, before Qt initialization; not part of the shipping app.
int nativeActivationPeerMain(int argc, char **argv);
} // namespace ux
