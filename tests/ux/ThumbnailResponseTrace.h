#pragma once
#if !defined(LUMA_UX_THUMBNAIL_TRACE)
#error "ThumbnailResponseTrace is test-target-only"
#endif
#include <QByteArray>
#include <QQuickImageResponse>
#include <QSize>
#include <QString>
#include <QUrl>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace ux::thumbnail_trace {
enum class Kind {
    Created,
    SnapshotRead,
    WorkerResultReady,
    GateEntered,
    GateReleased,
    GateTimedOut,
    GateSkipped,
    Cancelled,
    FinishedSignalObserved,
    TextureFactoryCalled,
    Destroyed
};
struct Request {
    std::uint64_t serial = 0;
    QString providerRequestID;
    QUrl fullURL;
    QSize requestedSize;
};
struct Event {
    std::uint64_t sequence = 0, serial = 0;
    Kind kind = Kind::Created;
    std::uint64_t revision = 0;
    quintptr threadID = 0;
    bool successfulPixels = false;
};
enum class GatePoint { BeforeFinishedQueue, AfterFirstCpuRow };
enum class CpuStage { Entered, FirstRowCompleted, Returned, Aborted };
struct CpuRenderEvent {
    std::uint64_t sequence = 0, serial = 0;
    CpuStage stage = CpuStage::Entered;
    std::uint64_t revision = 0;
    quintptr threadID = 0;
    int width = 0, height = 0, completedRows = 0;
    // Owned exact RGBA of the first completed row only; empty on other stages.
    QByteArray completedRowRGBA;
};
struct GateSpec {
    QUrl fullURL;
    QSize requestedSize;
    std::chrono::milliseconds timeout{15000};
    GatePoint point = GatePoint::BeforeFinishedQueue;
};
struct Snapshot {
    bool diagnosticFailure = false, gateEntered = false, gateTimedOut = false;
    bool releaseRequested = false, sessionClosing = false;
    std::uint64_t gatedSerial = 0;
    std::vector<Request> requests;
    std::vector<Event> events;
    std::vector<CpuRenderEvent> cpuRenders;
};
class State;
class Slot;
std::shared_ptr<Slot> makeSlot();
// Hooks below are called ONLY by the same production response/worker in the UX target.
// Scenarios may inspect Session but must never manually call Token/observeActualResponse.
class Token {
  public:
    Token() = default;
    void note(Kind kind, std::uint64_t revision = 0) const noexcept;
    void beforeFinishedQueue(bool successfulPixels) const noexcept;
    bool requiresPartialCpuProbe() const noexcept;
    void noteCpuStage(CpuStage stage, std::uint64_t revision, int width = 0,
                      int height = 0) const noexcept;
    void afterActualCpuRow(std::uint64_t revision, int width, int height,
                           std::span<const std::uint8_t> completedRow) const noexcept;

  private:
    Token(std::shared_ptr<State> state, std::uint64_t serial) noexcept;
    std::shared_ptr<State> state_;
    std::uint64_t serial_ = 0;
    friend Token observeActualResponse(const std::shared_ptr<Slot> &, QQuickImageResponse *,
                                       const QString &, const QSize &) noexcept;
};
class Session {
  public:
    explicit Session(std::shared_ptr<Slot> slot, std::optional<GateSpec> gate = std::nullopt);
    ~Session() noexcept;
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    Snapshot snapshot() const;
    void release() noexcept;

  private:
    std::shared_ptr<Slot> slot_;
    std::shared_ptr<State> state_;
};
Token observeActualResponse(const std::shared_ptr<Slot> &slot, QQuickImageResponse *response,
                            const QString &providerRequestID, const QSize &requestedSize) noexcept;
} // namespace ux::thumbnail_trace
