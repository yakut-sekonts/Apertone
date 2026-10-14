#pragma once
#include <CoreFoundation/CoreFoundation.h>
#include <utility>

namespace luma::macos {
template <class T> class CFHandle {
  public:
    explicit CFHandle(T value = nullptr) : value_(value) {}
    ~CFHandle() {
        if (value_)
            CFRelease(value_);
    }
    CFHandle(const CFHandle &) = delete;
    CFHandle &operator=(const CFHandle &) = delete;
    CFHandle(CFHandle &&other) noexcept : value_(std::exchange(other.value_, nullptr)) {}
    CFHandle &operator=(CFHandle &&other) noexcept {
        if (this != &other) {
            if (value_)
                CFRelease(value_);
            value_ = std::exchange(other.value_, nullptr);
        }
        return *this;
    }
    T get() const {
        return value_;
    }
    explicit operator bool() const {
        return value_ != nullptr;
    }

  private:
    T value_;
};
} // namespace luma::macos
