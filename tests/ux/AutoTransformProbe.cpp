#include "tests/ux/AutoTransformProbe.h"
#include "apps/desktop/EditorController.h"
#include <QThread>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

namespace ux {
struct AutoTransformProbeState {
    explicit AutoTransformProbeState(const EditorController *target) : controller(target) {}
    const EditorController *controller;
    mutable std::mutex mutex;
    std::condition_variable condition;
    AutoTransformProbeSnapshot observed;
    std::atomic_bool hookFailed{false};
    unsigned initialGlobalFailures = 0;
};
namespace {
std::mutex registryMutex;
std::weak_ptr<AutoTransformProbeState> activeGate;
std::atomic_uint globalFailures{0};
std::shared_ptr<AutoTransformProbeState> current(const AutoTransformOperation &operation) {
    std::lock_guard lock(registryMutex);
    const auto state = activeGate.lock();
    return state && state->controller == operation.controller ? state : nullptr;
}
} // namespace

bool AutoTransformOperation::sameAs(const AutoTransformOperation &other) const noexcept {
    return controller == other.controller && revision == other.revision &&
           editRevision == other.editRevision && loadRevision == other.loadRevision &&
           assetID == other.assetID && sourcePath == other.sourcePath &&
           job.get() == other.job.get();
}
AutoTransformResultGate::AutoTransformResultGate(EditorController &editor)
    : state_(std::make_shared<AutoTransformProbeState>(&editor)) {
    if (QThread::currentThread() != editor.thread())
        throw std::logic_error("Auto result gate must be owned by the editor GUI thread");
    std::lock_guard lock(registryMutex);
    if (!activeGate.expired())
        throw std::logic_error("Auto result gate cannot overlap another gate");
    state_->initialGlobalFailures = globalFailures.load();
    activeGate = state_;
}
AutoTransformResultGate::~AutoTransformResultGate() noexcept {
    // Never leave the worker waiting when an assertion unwinds before release.
    // The worker owns a shared State until it leaves the hold; no stack captures.
    release();
    try {
        std::lock_guard lock(registryMutex);
        if (activeGate.lock() == state_)
            activeGate.reset();
    } catch (...) {
        state_->hookFailed.store(true);
        ++globalFailures;
    }
}
AutoTransformProbeSnapshot AutoTransformResultGate::snapshot() const {
    std::lock_guard lock(state_->mutex);
    auto result = state_->observed;
    result.hookFailed =
        state_->hookFailed.load() || globalFailures.load() != state_->initialGlobalFailures;
    return result;
}
void AutoTransformResultGate::release() noexcept {
    try {
        {
            std::lock_guard lock(state_->mutex);
            state_->observed.released = true;
        }
        state_->condition.notify_all();
    } catch (...) {
        state_->hookFailed.store(true);
        ++globalFailures;
        state_->condition.notify_all();
    }
}
void holdAutoTransformSuccess(const AutoTransformOperation &operation,
                              const luma::TransformSettings &solved) noexcept {
    std::shared_ptr<AutoTransformProbeState> state;
    try {
        state = current(operation);
        if (!state)
            return;
        std::unique_lock lock(state->mutex);
        ++state->observed.successfulArrivals;
        if (state->observed.held || !operation.job || operation.assetID.empty() ||
            operation.sourcePath.empty()) {
            state->hookFailed.store(true);
            return;
        }
        state->observed.held = AutoTransformSuccess{operation, solved};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        if (!state->condition.wait_until(lock, deadline,
                                         [&] { return state->observed.released; })) {
            state->observed.timedOut = true;
            state->observed.released = true;
        }
        state->observed.leftHold = true;
    } catch (...) {
        if (state)
            state->hookFailed.store(true);
        ++globalFailures;
    }
}
void observeAutoTransformSuccessDelivery(const AutoTransformOperation &operation) noexcept {
    std::shared_ptr<AutoTransformProbeState> state;
    try {
        state = current(operation);
        if (!state)
            return;
        std::lock_guard lock(state->mutex);
        ++state->observed.deliveries;
        state->observed.lastDelivered = operation;
        if (!state->observed.released || !state->observed.leftHold)
            state->observed.deliveryBeforeRelease = true;
        if (!state->observed.held || !state->observed.held->operation.sameAs(operation))
            ++state->observed.mismatchedDeliveries;
    } catch (...) {
        if (state)
            state->hookFailed.store(true);
        ++globalFailures;
    }
}
void observeAutoTransformRevisionRejection(const AutoTransformOperation &operation,
                                           bool controllerPresent, std::uint64_t revision,
                                           std::uint64_t editRevision,
                                           std::uint64_t loadRevision) noexcept {
    std::shared_ptr<AutoTransformProbeState> state;
    try {
        state = current(operation);
        if (!state)
            return;
        std::lock_guard lock(state->mutex);
        ++state->observed.rejections;
        state->observed.rejection = AutoTransformRejection{operation, controllerPresent, revision,
                                                           editRevision, loadRevision};
        if (!state->observed.held || !state->observed.held->operation.sameAs(operation))
            ++state->observed.mismatchedDeliveries;
    } catch (...) {
        if (state)
            state->hookFailed.store(true);
        ++globalFailures;
    }
}
} // namespace ux
