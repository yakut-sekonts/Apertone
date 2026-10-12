#include "core/edits/EditDocument.h"
#include <array>
#include <string>

namespace luma {
namespace {
struct Parameter {
    std::string_view name;
    double EditDocument::*member;
    double minimum;
    double maximum;
    Correction correction;
};
constexpr std::array parameters{
    Parameter{"exposure", &EditDocument::exposure, -5, 5, Correction::exposure},
    Parameter{"temperature", &EditDocument::temperature, 2000, 25000, Correction::whiteBalance},
    Parameter{"tint", &EditDocument::tint, -150, 150, Correction::whiteBalance},
    Parameter{"contrast", &EditDocument::contrast, -100, 100, Correction::contrast},
    Parameter{"highlights", &EditDocument::highlights, -100, 100, Correction::highlights},
    Parameter{"shadows", &EditDocument::shadows, -100, 100, Correction::shadows},
    Parameter{"whites", &EditDocument::whites, -100, 100, Correction::whites},
    Parameter{"blacks", &EditDocument::blacks, -100, 100, Correction::blacks},
    Parameter{"vibrance", &EditDocument::vibrance, -100, 100, Correction::vibrance},
    Parameter{"saturation", &EditDocument::saturation, -100, 100, Correction::saturation}};
const Parameter &find(std::string_view name) {
    for (const auto &parameter : parameters) {
        if (parameter.name == name)
            return parameter;
    }
    throw std::runtime_error("Unknown edit parameter: " + std::string(name));
}
} // namespace
void EditDocument::validate() const {
    for (const auto &parameter : parameters) {
        const auto value = this->*parameter.member;
        if (!std::isfinite(value) || value < parameter.minimum || value > parameter.maximum)
            throw std::runtime_error("Invalid edit parameter: " + std::string(parameter.name));
    }
    if ((enabledCorrections & ~511U) != 0)
        throw std::runtime_error("Unknown enabled correction flags.");
}
double EditDocument::parameter(std::string_view name) const {
    return this->*find(name).member;
}
void EditDocument::setParameter(std::string_view name, double value) {
    auto next = *this;
    next.*find(name).member = value;
    if (name == "temperature" || name == "tint")
        next.customWhiteBalance = true;
    next.validate();
    *this = next;
}
Correction EditDocument::correction(std::string_view name) {
    return name == "whiteBalance" ? Correction::whiteBalance : find(name).correction;
}
} // namespace luma
