#include "tests/ux/ThumbnailResponseTrace.h"
#include <QThread>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace ux::thumbnail_trace {
class State {
  public:
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::atomic<bool> failed{false};
    std::optional<GateSpec> gate;
    std::vector<Request> requests;
    std::vector<Event> events;
    std::vector<CpuRenderEvent> cpuRenders;
    std::uint64_t nextSequence = 1, gatedSerial = 0;
    bool gateEntered = false, gateTimedOut = false, released = false, closing = false;
};
class Slot {
  public:
    std::mutex mutex;
    std::weak_ptr<State> installed;
    std::uint64_t nextSerial = 1;
    std::atomic<bool> failed{false};
};
std::shared_ptr<Slot> makeSlot() {
    return std::make_shared<Slot>();
}
namespace {
constexpr std::size_t maximumRequests = 4096, maximumEvents = 65536;
void appendLocked(State &state, std::uint64_t serial, Kind kind, std::uint64_t revision = 0,
                  bool successfulPixels = false) {
    if (state.events.size() + state.cpuRenders.size() >= maximumEvents) {
        state.failed.store(true);
        return;
    }
    state.events.push_back({state.nextSequence++, serial, kind, revision,
                            reinterpret_cast<quintptr>(QThread::currentThreadId()),
                            successfulPixels});
}
} // namespace
Token::Token(std::shared_ptr<State> state, std::uint64_t serial) noexcept
    : state_(std::move(state)), serial_(serial) {}
void Token::note(Kind kind, std::uint64_t revision) const noexcept {
    if (!state_)
        return;
    try {
        std::lock_guard lock(state_->mutex);
        appendLocked(*state_, serial_, kind, revision);
    } catch (...) {
        state_->failed.store(true);
    }
}
void Token::beforeFinishedQueue(bool successfulPixels) const noexcept {
    if (!state_)
        return;
    try {
        std::unique_lock lock(state_->mutex);
        appendLocked(*state_, serial_, Kind::WorkerResultReady, 0, successfulPixels);
        if (state_->gatedSerial != serial_ || state_->gate->point != GatePoint::BeforeFinishedQueue)
            return;
        if (!successfulPixels || state_->released || state_->closing) {
            appendLocked(*state_, serial_, Kind::GateSkipped);
            return;
        }
        state_->gateEntered = true;
        appendLocked(*state_, serial_, Kind::GateEntered);
        // This runs ONLY on one provider pool worker, after all provider mutex scopes.
        // condition_variable wait releases the trace mutex; Qt reader/GUI never wait here.
        const auto deadline = std::chrono::steady_clock::now() + state_->gate->timeout;
        if (state_->changed.wait_until(lock, deadline,
                                       [&] { return state_->released || state_->closing; })) {
            appendLocked(*state_, serial_, Kind::GateReleased);
        } else {
            state_->gateTimedOut = true;
            state_->failed.store(true);
            appendLocked(*state_, serial_, Kind::GateTimedOut);
        }
    } catch (...) {
        // Fail the evidence, then preserve the real worker's original completion path.
        state_->failed.store(true);
    }
}
bool Token::requiresPartialCpuProbe() const noexcept {
    if (!state_)
        return false;
    try {
        std::lock_guard lock(state_->mutex);
        return state_->gatedSerial == serial_ && state_->gate &&
               state_->gate->point == GatePoint::AfterFirstCpuRow && !state_->released &&
               !state_->closing;
    } catch (...) {
        state_->failed.store(true);
        return false;
    }
}
void Token::noteCpuStage(CpuStage stage, std::uint64_t revision, int width,
                         int height) const noexcept {
    if (!state_)
        return;
    try {
        std::lock_guard lock(state_->mutex);
        if (state_->gatedSerial != serial_ || !state_->gate ||
            state_->gate->point != GatePoint::AfterFirstCpuRow)
            return;
        if (stage == CpuStage::FirstRowCompleted ||
            state_->events.size() + state_->cpuRenders.size() >= maximumEvents) {
            state_->failed.store(true);
            return;
        }
        state_->cpuRenders.push_back({state_->nextSequence++,
                                      serial_,
                                      stage,
                                      revision,
                                      reinterpret_cast<quintptr>(QThread::currentThreadId()),
                                      width,
                                      height,
                                      0,
                                      {}});
    } catch (...) {
        state_->failed.store(true);
    }
}
void Token::afterActualCpuRow(std::uint64_t revision, int width, int height,
                              std::span<const std::uint8_t> completedRow) const noexcept {
    if (!state_)
        return;
    try {
        std::unique_lock lock(state_->mutex);
        if (state_->gatedSerial != serial_ || !state_->gate ||
            state_->gate->point != GatePoint::AfterFirstCpuRow)
            return;
        // A one-row image is already complete and cannot prove this boundary.
        // Invalid evidence fails closed, while the original renderer continues.
        if (width < 1 || width > 384 || height < 2 || height > 384 ||
            completedRow.size() != static_cast<std::size_t>(width) * 4 || state_->gateEntered ||
            state_->events.size() + state_->cpuRenders.size() >= maximumEvents) {
            state_->failed.store(true);
            return;
        }
        state_->cpuRenders.push_back(
            {state_->nextSequence++, serial_, CpuStage::FirstRowCompleted, revision,
             reinterpret_cast<quintptr>(QThread::currentThreadId()), width, height, 1,
             QByteArray(reinterpret_cast<const char *>(completedRow.data()),
                        static_cast<qsizetype>(completedRow.size()))});
        if (state_->released || state_->closing) {
            appendLocked(*state_, serial_, Kind::GateSkipped, revision);
            return;
        }
        state_->gateEntered = true;
        appendLocked(*state_, serial_, Kind::GateEntered, revision);
        // Actual renderCPU stack, after one complete row and before later rows.
        // No SQL/cache/state locks are held. wait_until releases this trace mutex.
        const auto deadline = std::chrono::steady_clock::now() + state_->gate->timeout;
        if (state_->changed.wait_until(lock, deadline,
                                       [&] { return state_->released || state_->closing; })) {
            appendLocked(*state_, serial_, Kind::GateReleased, revision);
        } else {
            state_->gateTimedOut = true;
            state_->failed.store(true);
            appendLocked(*state_, serial_, Kind::GateTimedOut, revision);
        }
        // No cancellation override/check is inserted: renderer's next original
        // row begins with checkCancellation(stop), preserving the actual outcome.
    } catch (...) {
        state_->failed.store(true);
    }
}
Session::Session(std::shared_ptr<Slot> slot, std::optional<GateSpec> gate)
    : slot_(std::move(slot)), state_(std::make_shared<State>()) {
    if (!slot_)
        throw std::invalid_argument("Missing actual provider trace Slot");
    if (gate && (!gate->fullURL.isValid() || gate->fullURL.scheme() != "image" ||
                 gate->fullURL.host() != "thumbnail" || gate->fullURL.path().isEmpty() ||
                 !gate->requestedSize.isValid() || gate->timeout.count() < 1 ||
                 gate->timeout > std::chrono::milliseconds(30000) ||
                 (gate->point != GatePoint::BeforeFinishedQueue &&
                  gate->point != GatePoint::AfterFirstCpuRow)))
        throw std::invalid_argument("Invalid or unbounded actual thumbnail gate");
    state_->gate = std::move(gate);
    std::lock_guard lock(slot_->mutex);
    if (!slot_->installed.expired())
        throw std::logic_error("Only one actual thumbnail trace Session may bind this provider");
    slot_->failed.store(false);
    slot_->installed = state_;
}
Session::~Session() noexcept {
    try {
        {
            std::lock_guard lock(state_->mutex);
            state_->closing = true;
            state_->released = true;
        }
        state_->changed.notify_all();
        std::lock_guard lock(slot_->mutex);
        if (slot_->installed.lock() == state_)
            slot_->installed.reset();
    } catch (...) {
        // A broken diagnostic lock cannot escape. Any gate still has its finite deadline.
        state_->failed.store(true);
        state_->changed.notify_all();
    }
}
void Session::release() noexcept {
    try {
        {
            std::lock_guard lock(state_->mutex);
            state_->released = true;
        }
        state_->changed.notify_all();
    } catch (...) {
        state_->failed.store(true);
        state_->changed.notify_all();
    }
}
Snapshot Session::snapshot() const {
    std::lock_guard lock(state_->mutex);
    return {state_->failed.load() || slot_->failed.load(),
            state_->gateEntered,
            state_->gateTimedOut,
            state_->released,
            state_->closing,
            state_->gatedSerial,
            state_->requests,
            state_->events,
            state_->cpuRenders};
}
Token observeActualResponse(const std::shared_ptr<Slot> &slot, QQuickImageResponse *response,
                            const QString &providerRequestID, const QSize &requestedSize) noexcept {
    std::shared_ptr<State> state;
    try {
        if (!slot)
            return {};
        std::uint64_t serial = 0;
        {
            std::lock_guard lock(slot->mutex);
            state = slot->installed.lock();
            if (!state)
                return {};
            serial = slot->nextSerial++;
        }
        {
            std::lock_guard lock(state->mutex);
            if (state->requests.size() == maximumRequests) {
                state->failed.store(true);
                return {};
            }
            if (!serial) {
                state->failed.store(true);
                return {};
            }
            const QUrl fullURL(QStringLiteral("image://thumbnail/") + providerRequestID);
            state->requests.push_back({serial, providerRequestID, fullURL, requestedSize});
            if (state->gate && !state->gatedSerial && !state->closing &&
                fullURL == state->gate->fullURL && requestedSize == state->gate->requestedSize)
                state->gatedSerial = serial;
            appendLocked(*state, serial, Kind::Created);
        }
        const Token token(state, serial);
        // Stable external state + serial only: neither callback captures response/this.
        const auto finished = QObject::connect(
            response, &QQuickImageResponse::finished, response,
            [token]() noexcept { token.note(Kind::FinishedSignalObserved); }, Qt::DirectConnection);
        const auto destroyed =
            QObject::connect(response, &QObject::destroyed,
                             [token](QObject *) noexcept { token.note(Kind::Destroyed); });
        if (!finished || !destroyed)
            state->failed.store(true);
        return token;
    } catch (...) {
        if (state)
            state->failed.store(true);
        else if (slot)
            slot->failed.store(true);
        return {};
    }
}
} // namespace ux::thumbnail_trace
