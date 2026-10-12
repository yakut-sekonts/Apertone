#pragma once
#include <atomic>
#include <memory>

namespace luma {
class CancellationSource;

/// Cooperative cancellation without requiring libc++'s optional C++20 stop_token.
/// Apple Xcode 16 supports C++20 but does not yet provide that library facility.
class CancellationToken {
  public:
    CancellationToken() = default;
    bool isCancelled() const noexcept {
        return state_ && state_->load(std::memory_order_acquire);
    }

  private:
    friend class CancellationSource;
    explicit CancellationToken(std::shared_ptr<std::atomic<bool>> state)
        : state_(std::move(state)) {}
    std::shared_ptr<std::atomic<bool>> state_;
};

class CancellationSource {
  public:
    bool cancel() const noexcept {
        return !state_->exchange(true, std::memory_order_acq_rel);
    }
    bool isCancelled() const noexcept {
        return state_->load(std::memory_order_acquire);
    }
    CancellationToken token() const {
        return CancellationToken(state_);
    }

  private:
    std::shared_ptr<std::atomic<bool>> state_ = std::make_shared<std::atomic<bool>>(false);
};
} // namespace luma
