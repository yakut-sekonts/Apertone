#pragma once
#include <stdexcept>

namespace luma {
enum class ExportFormat { jpeg, png, tiff16 };
struct ExportOptions {
    ExportFormat format = ExportFormat::jpeg;
    int quality = 92;
    bool overwrite = false;
    void validate() const {
        if (quality < 1 || quality > 100)
            throw std::runtime_error("JPEG quality must be 1–100.");
        if (format != ExportFormat::jpeg && format != ExportFormat::png &&
            format != ExportFormat::tiff16)
            throw std::runtime_error("Unsupported export format.");
    }
};
} // namespace luma
