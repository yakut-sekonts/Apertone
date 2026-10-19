#pragma once
#include "core/edits/TransformSettings.h"
#include "core/jobs/JobManager.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

class EditorController;

namespace ux {
// Test-target-only observation. No production QObject properties or ABI changes.
struct AutoTransformOperation {
    const EditorController *controller = nullptr;
    std::uint64_t revision = 0, editRevision = 0, loadRevision = 0;
    std::string assetID, sourcePath;
    std::shared_ptr<const luma::JobData> job;
    bool sameAs(const AutoTransformOperation &other) const noexcept;
};
struct AutoTransformSuccess {
    AutoTransformOperation operation;
    luma::TransformSettings solved;
};
struct AutoTransformRejection {
    AutoTransformOperation operation;
    bool controllerPresent = false;
    std::uint64_t currentRevision = 0, currentEditRevision = 0, currentLoadRevision = 0;
};
struct AutoTransformProbeSnapshot {
    std::optional<AutoTransformSuccess> held;
    std::optional<AutoTransformOperation> lastDelivered;
    std::optional<AutoTransformRejection> rejection;
    unsigned successfulArrivals = 0, deliveries = 0, rejections = 0, mismatchedDeliveries = 0;
    bool released = false, leftHold = false, timedOut = false;
    bool deliveryBeforeRelease = false, hookFailed = false;
};
struct AutoTransformProbeState;
class AutoTransformResultGate final {
  public:
    explicit AutoTransformResultGate(EditorController &editor);
    ~AutoTransformResultGate() noexcept;
    AutoTransformResultGate(const AutoTransformResultGate &) = delete;
    AutoTransformResultGate &operator=(const AutoTransformResultGate &) = delete;
    AutoTransformProbeSnapshot snapshot() const;
    void release() noexcept;

  private:
    std::shared_ptr<AutoTransformProbeState> state_;
};

// Runs on the actual transform worker AFTER solve succeeded and BEFORE the
// original success invokeMethod. Does not execute callbacks or change the result.
// Always releases within 30s, including a test that fails to call release().
void holdAutoTransformSuccess(const AutoTransformOperation &operation,
                              const luma::TransformSettings &solved) noexcept;
// Passive tap at entry to the original success lambda, before its existing guard.
void observeAutoTransformSuccessDelivery(const AutoTransformOperation &operation) noexcept;
// Called only INSIDE the existing !self || revision-mismatch return branch.
// Current values are copied by that GUI lambda; the worker never reads them.
void observeAutoTransformRevisionRejection(const AutoTransformOperation &operation,
                                           bool controllerPresent, std::uint64_t revision,
                                           std::uint64_t editRevision,
                                           std::uint64_t loadRevision) noexcept;
} // namespace ux
