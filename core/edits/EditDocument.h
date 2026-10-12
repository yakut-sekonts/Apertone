#pragma once
#include <cmath>
#include <stdexcept>

namespace luma {
struct EditDocument {
    double exposure = 0;
    void validate() const {
        if (!std::isfinite(exposure) || exposure < -5 || exposure > 5) {
            throw std::runtime_error("Exposure must be finite and between -5 and +5 EV.");
        }
    }
};
} // namespace luma
