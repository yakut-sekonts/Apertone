#pragma once
#include <QRectF>
#include <QString>
#include <memory>

class QQuickWindow;

namespace ux {
// Public AppKit operations. No launch, OS setting, CGEvent or activation-policy change.
struct NativeFinderState {
    bool applicationActive = false, applicationHidden = false;
    bool ownRunningActive = false, finderActive = false, finderTerminated = false;
    bool ownFrontmost = false, finderFrontmost = false;
    bool windowKey = false, windowVisible = false, windowMiniaturized = false;
    int ownPID = 0, finderPID = 0, windowNumber = 0, activationPolicy = 0;
    unsigned appResigned = 0, appActivated = 0, windowResignedKey = 0;
    unsigned finderActivated = 0, ownActivated = 0;
};

// Passive identity/geometry for external native UI targeting. Coordinates are
// AppKit global points, not CUA screen pixels; no input or activation is performed.
struct NativeFinderIdentity {
    int ownPID = 0, windowNumber = 0;
    QString executablePath, bundleIdentifier, bundlePath, localizedName, windowTitle;
    QRectF windowFrameAppKit, screenFrameAppKit;
};

class NativeFinderActivation final {
  public:
    explicit NativeFinderActivation(QQuickWindow &window);
    ~NativeFinderActivation();
    NativeFinderActivation(const NativeFinderActivation &) = delete;
    NativeFinderActivation &operator=(const NativeFinderActivation &) = delete;
    NativeFinderState state() const;
    NativeFinderIdentity identity() const;
    // Dispatch result is never activation proof; independently observe fresh native/Qt state.
    bool requestFinderActivation();
    void requestOwnActivation();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ux
