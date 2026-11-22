#include "core/image/Bayer.h"
#include "core/merge/MergeAlignment.h"
#include "platform/macos/CoreMLDenoise.h"
#include "platform/macos/CoreMLSky.h"
#include "platform/macos/DisplayProfile.h"
#include "platform/macos/FileHasher.h"
#include "platform/macos/ImageAlignment.h"
#include "platform/macos/MemoryPressure.h"
#include "platform/macos/MetalDevelop.h"
#include "platform/macos/PhotoCulling.h"
#include "platform/macos/SemanticEmbedding.h"
#include "platform/macos/SystemDiagnostics.h"
#include "platform/macos/VisionSegmentation.h"
#include "platform/windows/OnnxSegmentation.h"
#include <QCryptographicHash>
#include <QFile>
#include <QImage>
#include <QSysInfo>
#include <array>
#include <stdexcept>
#include <thread>
#include <windows.h>

#include <psapi.h>

namespace luma::macos {
std::string sha256File(const std::filesystem::path &path, CancellationToken stop) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("Cannot open file for SHA-256.");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::array<char, 256 * 1024> buffer{};
    while (true) {
        checkCancellation(stop);
        const auto n = file.read(buffer.data(), static_cast<qint64>(buffer.size()));
        if (n < 0)
            throw std::runtime_error("Cannot read file for SHA-256.");
        if (n == 0)
            return hash.result().toHex().toStdString();
        hash.addData(QByteArrayView(buffer.data(), n));
    }
}

std::vector<std::uint8_t> displayProfile(std::uintptr_t nativeView) {
    const auto window = reinterpret_cast<HWND>(nativeView);
    const auto monitor = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW info{};
    info.cbSize = sizeof info;
    if (!GetMonitorInfoW(monitor, reinterpret_cast<MONITORINFO *>(&info)))
        return {};
    const auto dc = CreateDCW(L"DISPLAY", info.szDevice, nullptr, nullptr);
    if (!dc)
        return {};
    std::array<wchar_t, 4096> name{};
    DWORD size = static_cast<DWORD>(name.size());
    const bool found = GetICMProfileW(dc, &size, name.data());
    DeleteDC(dc);
    if (!found)
        return {};
    QFile file(QString::fromWCharArray(name.data()));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024)
        return {};
    const auto data = file.readAll();
    return {data.begin(), data.end()};
}

struct MemoryPressureMonitor::State {
    HANDLE pressure = CreateMemoryResourceNotification(LowMemoryResourceNotification);
    HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::thread worker;
    ~State() {
        if (worker.joinable()) {
            SetEvent(stop);
            worker.join();
        }
        if (stop)
            CloseHandle(stop);
        if (pressure)
            CloseHandle(pressure);
    }
};

MemoryPressureMonitor::MemoryPressureMonitor(std::function<void(MemoryPressure)> callback)
    : state_(std::make_unique<State>()) {
    if (!state_->pressure || !state_->stop)
        return;
    state_->worker = std::thread([state = state_.get(), callback = std::move(callback)] {
        bool low = false;
        while (WaitForSingleObject(state->stop, 1000) == WAIT_TIMEOUT) {
            BOOL current = FALSE;
            if (QueryMemoryResourceNotification(state->pressure, &current) &&
                low != bool(current)) {
                low = current;
                callback(low ? MemoryPressure::critical : MemoryPressure::normal);
            }
        }
    });
}
MemoryPressureMonitor::~MemoryPressureMonitor() = default;

SystemDiagnostics systemDiagnostics() {
    SystemDiagnostics info;
    info.cpu = QSysInfo::currentCpuArchitecture().toStdString();
    info.logicalCores = static_cast<int>(std::thread::hardware_concurrency());
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof memory;
    if (GlobalMemoryStatusEx(&memory))
        info.physicalMemory = memory.ullTotalPhys;
    PROCESS_MEMORY_COUNTERS process{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &process, sizeof process))
        info.residentMemory = process.WorkingSetSize;
    return info;
}

std::shared_ptr<DevelopAccelerator> makeMetalDevelopAccelerator() {
    return {};
}
std::shared_ptr<ai::ISegmentationProvider> makeVisionSegmentation() {
    return windows::makeOnnxSegmentation();
}
std::shared_ptr<ai::ISegmentationProvider> makeCoreMLSky(const std::filesystem::path &) {
    return {};
}
std::shared_ptr<ai::IDenoiseProvider> makeCoreMLDenoise(const std::filesystem::path &path) {
    return windows::makeOnnxDenoise(path);
}
namespace {
class PreviewCulling final : public ai::ICullingProvider {
  public:
    std::string identifier() const override {
        return "apertone.culling-windows-v1.grid16";
    }
    ai::ExecutionLocation location() const override {
        return ai::ExecutionLocation::local;
    }
    ai::CullingObservation analyze(ai::RgbaImage input, CancellationToken stop,
                                   const ai::Progress &progress) override {
        input.validate();
        checkCancellation(stop);
        const QImage preview(input.pixels.data(), input.width, input.height, input.width * 4,
                             QImage::Format_RGBA8888);
        constexpr int side = 16;
        const auto reduced =
            preview.convertToFormat(QImage::Format_RGBA8888_Premultiplied)
                .scaled(side, side, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (reduced.isNull())
            throw std::runtime_error("Cannot allocate the culling preview.");
        std::vector<float> descriptor;
        descriptor.reserve(side * side + 1);
        for (int y = 0; y < side; ++y) {
            const auto *row = reduced.constScanLine(y);
            for (int x = 0; x < side; ++x)
                descriptor.push_back(
                    (.2126F * row[x * 4] + .7152F * row[x * 4 + 1] + .0722F * row[x * 4 + 2]) /
                    255.F);
        }
        descriptor.push_back(.25F);
        auto scored = ai::scoreCullingPreview(input, std::move(descriptor), {}, stop);
        if (progress)
            progress(1);
        return scored;
    }
};
} // namespace

std::shared_ptr<ai::ICullingProvider> makePhotoCulling() {
    return std::make_shared<PreviewCulling>();
}
std::shared_ptr<ai::IEmbeddingProvider> makeSemanticEmbedding() {
    return {};
}

Homography alignImages(ai::RgbaImage reference, ai::RgbaImage floating, CancellationToken stop,
                       AlignmentMode) {
    return verifiedMergeAlignment(reference, floating, {}, stop);
}
} // namespace luma::macos
