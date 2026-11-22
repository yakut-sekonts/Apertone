#include "platform/windows/CameraPtpData.h"
#include <iostream>
#include <stdexcept>

namespace {
using Bytes = std::vector<std::uint8_t>;
using namespace luma::windows::ptp;

void check(bool valid, const char *message) {
    if (!valid)
        throw std::runtime_error(message);
}

void rejects(const auto &operation) {
    try {
        operation();
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error("An invalid camera dataset or exposure request was accepted.");
}

void append(Bytes &bytes, unsigned value, unsigned width = 2) {
    for (unsigned i = 0; i < width; ++i)
        bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

void verifyDeviceInfo() {
    Bytes dataset{100, 0, 10, 0, 0, 0, 100, 0, 0, 0, 0};
    append(dataset, 3, 4);
    for (const auto operation : {0x9201, 0x9202, 0x9203})
        append(dataset, operation);
    append(dataset, 0, 4);
    append(dataset, 2, 4);
    append(dataset, 0x5007);
    append(dataset, 0x500f);
    append(dataset, 0, 4);
    append(dataset, 0, 4);
    append(dataset, 6, 1);
    for (const auto letter : {u'N', u'i', u'k', u'o', u'n', u'\0'})
        append(dataset, letter);
    const auto info = deviceInfo(dataset);
    check(info.nikon() && info.supports(0x9203) && info.hasProperty(0x500f) &&
              !info.hasProperty(0x5010),
          "Nikon Live View or the reported properties were lost.");
    dataset.pop_back();
    rejects([&] { (void)deviceInfo(dataset); });
}

void verifyExposure() {
    const Bytes compensation{0x10, 0x50, 3, 0,    1,    0, 0, 0x18, 0xfc,
                             2,    3,    0, 0x18, 0xfc, 0, 0, 0xe8, 0x03};
    const auto property = exposureProperty(compensation, 0x5010);
    check(property.current == -1000 &&
              property.choices == std::vector<std::int64_t>{-1000, 0, 1000},
          "Signed PTP exposure compensation values were lost.");
    check(exposureValue(property, -1000) == Bytes{0x18, 0xfc},
          "The selected exposure compensation was serialized incorrectly.");
    rejects([&] { (void)exposureValue(property, 500); });
    rejects([&] { (void)exposureProperty(compensation, 0x5007); });

    auto fixed = compensation;
    fixed[4] = 0;
    const auto readOnly = exposureProperty(fixed, 0x5010);
    check(!readOnly.writable, "The camera's read-only mode was lost.");
    rejects([&] { (void)exposureValue(readOnly, 1000); });

    Bytes shutter{0x0d, 0x50, 6, 0, 1};
    append(shutter, 250, 4);
    append(shutter, 500, 4);
    append(shutter, 1, 1);
    append(shutter, 250, 4);
    append(shutter, 1000, 4);
    append(shutter, 250, 4);
    const auto range = exposureProperty(shutter, 0x500d);
    check(range.choices == std::vector<std::int64_t>{250, 500, 750, 1000} &&
              exposureValue(range, 750) == Bytes{0xee, 0x02, 0, 0},
          "The camera shutter speed range or its byte encoding changed.");
    shutter.pop_back();
    rejects([&] { (void)exposureProperty(shutter, 0x500d); });
}
} // namespace

int main() {
    try {
        verifyDeviceInfo();
        verifyExposure();
        std::cout << "PTP Live View discovery and safe exposure encoding passed.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
