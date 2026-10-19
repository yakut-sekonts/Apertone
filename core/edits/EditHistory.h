#pragma once
#include "core/edits/EditDocument.h"
#include <optional>
#include <vector>

namespace luma {
class EditHistory {
  public:
    explicit EditHistory(EditDocument initial = {}) : current_(initial) {
        initial.validate();
    }
    const EditDocument &current() const {
        return current_;
    }
    // Pure observation: does not finish or mutate the current gesture.
    bool gestureActive() const noexcept {
        return gesture_.has_value();
    }
    bool canUndo() const {
        return !undo_.empty() || (gesture_ && *gesture_ != current_);
    }
    bool canRedo() const {
        return !redo_.empty() && !gesture_;
    }
    void begin() {
        if (!gesture_)
            gesture_ = current_;
    }
    void end() {
        if (gesture_) {
            if (*gesture_ != current_) {
                push(undo_, *gesture_);
                redo_.clear();
            }
            gesture_.reset();
        }
    }
    void apply(EditDocument next) {
        next.validate();
        if (next == current_)
            return;
        if (!gesture_) {
            push(undo_, current_);
            redo_.clear();
        }
        current_ = next;
    }
    void undo() {
        end();
        if (undo_.empty())
            return;
        push(redo_, current_);
        current_ = undo_.back();
        undo_.pop_back();
    }
    void redo() {
        end();
        if (redo_.empty())
            return;
        push(undo_, current_);
        current_ = redo_.back();
        redo_.pop_back();
    }

  private:
    EditDocument current_;
    std::vector<EditDocument> undo_, redo_;
    std::optional<EditDocument> gesture_;
    static void push(std::vector<EditDocument> &stack, EditDocument value) {
        if (stack.size() == 200)
            stack.erase(stack.begin());
        stack.push_back(value);
    }
};
} // namespace luma
