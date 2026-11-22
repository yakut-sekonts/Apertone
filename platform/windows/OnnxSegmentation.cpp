#include "platform/windows/OnnxSegmentation.h"
#include "core/image/Bayer.h"
#include "core/io/Path.h"
#include <QCoreApplication>
#include <QImage>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <mutex>
#include <optional>
#include <type_traits>
#include <windows.h>

// ONNX Runtime's Windows header uses MSVC's alternate spelling.
#if defined(__MINGW32__) && !defined(_stdcall)
#define _stdcall __stdcall
#endif
#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>

namespace luma::windows {
namespace {
constexpr std::array modelNames{L"u2netp.onnx", L"skyseg.onnx", L"pphumanseg.onnx"};

std::filesystem::path modelHome() {
    const auto app = pathFromUtf8(QCoreApplication::applicationDirPath().toStdString());
    for (const auto &candidate : {app, app.parent_path()}) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate / L"onnxruntime.dll", error) &&
            std::filesystem::is_directory(candidate / L"Models", error))
            return candidate;
    }
    return {};
}

class Runtime {
  public:
    explicit Runtime(const std::filesystem::path &home) : library_(load(home)), env_(initialize()) {
        env_.DisableTelemetryEvents();
    }
    Ort::Env &env() { return env_; }

  private:
    struct Free {
        void operator()(HMODULE module) const { FreeLibrary(module); }
    };
    using Library = std::unique_ptr<std::remove_pointer_t<HMODULE>, Free>;
    static Library load(const std::filesystem::path &home) {
        Library library(LoadLibraryExW((home / L"onnxruntime.dll").c_str(), nullptr,
                                       LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32));
        if (!library)
            throw std::runtime_error("Cannot load ONNX Runtime. Install the x64 Microsoft Visual C++ Redistributable.");
        return library;
    }
    Ort::Env initialize() {
        const auto symbol = GetProcAddress(library_.get(), "OrtGetApiBase");
        if (!symbol)
            throw std::runtime_error("ONNX Runtime does not expose its C API.");
        const auto base = std::bit_cast<decltype(&OrtGetApiBase)>(symbol)();
        const auto api = base ? base->GetApi(ORT_API_VERSION) : nullptr;
        if (!api)
            throw std::runtime_error("Incompatible ONNX Runtime version.");
        Ort::InitApi(api);
        return Ort::Env(ORT_LOGGING_LEVEL_WARNING, "ApertoneLocalModels");
    }
    Library library_;
    Ort::Env env_;
};

Ort::SessionOptions options() {
    Ort::SessionOptions result;
    result.SetIntraOpNumThreads(4);
    result.SetInterOpNumThreads(1);
    result.SetGraphOptimizationLevel(ORT_ENABLE_EXTENDED);
    return result;
}

struct Model {
    Model(Runtime &runtime, const std::filesystem::path &path, int edge, int channels)
        : session(runtime.env(), path.c_str(), options()), edge(edge), channels(channels) {
        Ort::AllocatorWithDefaultOptions allocator;
        const auto input = session.GetInputNameAllocated(0, allocator);
        const auto output = session.GetOutputNameAllocated(0, allocator);
        inputName = input.get();
        outputName = output.get();
        const auto inputInfo = session.GetInputTypeInfo(0);
        const auto outputInfo = session.GetOutputTypeInfo(0);
        const auto in = inputInfo.GetTensorTypeAndShapeInfo();
        const auto out = outputInfo.GetTensorTypeAndShapeInfo();
        if (session.GetInputCount() != 1 ||
            in.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
            out.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
            in.GetShape() != std::vector<std::int64_t>{1, 3, edge, edge} ||
            out.GetShape() != std::vector<std::int64_t>{1, channels, edge, edge})
            throw std::runtime_error("The bundled segmentation model has incompatible tensors.");
    }

    std::vector<std::uint8_t> infer(ai::RgbaImage image, CancellationToken stop) {
        const QImage input(image.pixels.data(), image.width, image.height,
                           static_cast<qsizetype>(image.width) * 4, QImage::Format_RGBA8888);
        const auto resized = input.scaled(edge, edge, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (resized.isNull())
            throw std::runtime_error("Cannot allocate the segmentation input.");
        constexpr float mean[]{.485F, .456F, .406F}, stddev[]{.229F, .224F, .225F};
        const auto count = pixelCount(edge, edge);
        std::vector<float> rgb(count * 3);
        for (int y = 0; y < edge; ++y) {
            checkCancellation(stop);
            const auto *row = resized.constScanLine(y);
            for (int x = 0; x < edge; ++x) {
                const auto offset = static_cast<std::size_t>(y) * edge + x;
                const float alpha = row[4 * x + 3] / 255.F;
                for (int c = 0; c < 3; ++c) {
                    const float value = alpha * (row[4 * x + c] / 255.F);
                    rgb[c * count + offset] =
                        channels == 2 ? value * 2.F - 1.F : (value - mean[c]) / stddev[c];
                }
            }
        }
        const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<std::int64_t, 4> shape{1, 3, edge, edge};
        const auto tensor = Ort::Value::CreateTensor<float>(memory, rgb.data(), rgb.size(),
                                                            shape.data(), shape.size());
        const char *inputs[]{inputName.c_str()}, *outputs[]{outputName.c_str()};
        checkCancellation(stop);
        const auto prediction = session.Run(Ort::RunOptions{}, inputs, &tensor, 1, outputs, 1);
        checkCancellation(stop);
        const auto *probability = prediction.front().GetTensorData<float>() + (channels == 2 ? count : 0);
        std::vector<std::uint8_t> mask(count);
        for (std::size_t i = 0; i < count; ++i) {
            if (!std::isfinite(probability[i]))
                throw std::runtime_error("The local model produced invalid mask values.");
            const auto normalized = std::clamp((probability[i] - .1F) / .85F, 0.F, 1.F);
            mask[i] = static_cast<std::uint8_t>(std::lround(255.F * normalized));
        }
        return mask;
    }

    Ort::Session session;
    std::string inputName, outputName;
    int edge, channels;
};

void keepComponent(std::vector<std::uint8_t> &mask, int edge, std::array<double, 2> point,
                   CancellationToken stop) {
    if (!std::isfinite(point[0]) || !std::isfinite(point[1]) ||
        point[0] < 0 || point[1] < 0 || point[0] > 1 || point[1] > 1)
        throw std::runtime_error("Invalid location for an object mask.");
    const auto x = std::min(edge - 1, static_cast<int>(point[0] * edge));
    const auto y = std::min(edge - 1, static_cast<int>(point[1] * edge));
    const int seed = y * edge + x;
    if (mask[seed] < 128)
        throw std::runtime_error("No foreground object was detected at that point.");
    std::vector<std::uint8_t> selected(mask.size());
    std::vector<int> queue{seed};
    selected[seed] = mask[seed];
    for (std::size_t i = 0; i < queue.size(); ++i) {
        if (i % 256 == 0)
            checkCancellation(stop);
        const auto index = queue[i];
        const auto visit = [&](int neighbor) {
            if (mask[neighbor] && !selected[neighbor]) {
                selected[neighbor] = mask[neighbor];
                if (mask[neighbor] >= 128)
                    queue.push_back(neighbor);
            }
        };
        if (index % edge > 0) visit(index - 1);
        if (index % edge < edge - 1) visit(index + 1);
        if (index >= edge) visit(index - edge);
        if (index < edge * (edge - 1)) visit(index + edge);
    }
    mask.swap(selected);
}

class OnnxSegmentation final : public ai::ISegmentationProvider {
  public:
    explicit OnnxSegmentation(std::filesystem::path home) : home_(std::move(home)) {}
    std::string identifier() const override { return "onnx.windows.segmentation.v1"; }
    ai::ExecutionLocation location() const override { return ai::ExecutionLocation::local; }
    bool supports(ai::Selection selection) const override {
        const auto index = modelIndex(selection);
        std::error_code error;
        return index && std::filesystem::is_regular_file(home_ / L"Models" / modelNames[*index], error);
    }
    BitmapMask segment(ai::RgbaImage image, ai::Selection selection, std::array<double, 2> point,
                       CancellationToken stop, const ai::Progress &progress) override {
        image.validate();
        checkCancellation(stop);
        if (!supports(selection))
            throw std::runtime_error("The requested AI selection is not available locally.");
        if (progress)
            progress(.1);
        auto &model = getModel(*modelIndex(selection));
        if (progress)
            progress(.3);
        auto result = model.infer(image, stop);
        if (selection == ai::Selection::object)
            keepComponent(result, model.edge, point, stop);
        if (selection == ai::Selection::background)
            for (auto &value : result)
                value = static_cast<std::uint8_t>(255 - value);
        if (*std::ranges::max_element(result) < 128)
            throw std::runtime_error("The local model found no matching region.");
        if (progress)
            progress(1);
        return BitmapMask(model.edge, model.edge, std::move(result));
    }

  private:
    static std::optional<std::size_t> modelIndex(ai::Selection selection) {
        switch (selection) {
        case ai::Selection::subject:
        case ai::Selection::background:
        case ai::Selection::object: return 0;
        case ai::Selection::sky: return 1;
        case ai::Selection::people: return 2;
        default: return std::nullopt;
        }
    }
    Model &getModel(std::size_t index) {
        std::lock_guard lock(mutex_);
        static Runtime runtime(home_);
        if (!models_[index])
            models_[index] = std::make_unique<Model>(
                runtime, home_ / L"Models" / modelNames[index], index == 2 ? 192 : 320, index == 2 ? 2 : 1);
        return *models_[index];
    }
    std::filesystem::path home_;
    std::mutex mutex_;
    std::array<std::unique_ptr<Model>, modelNames.size()> models_;
};
} // namespace

std::shared_ptr<ai::ISegmentationProvider> makeOnnxSegmentation() {
    const auto home = modelHome();
    return home.empty() ? nullptr : std::make_shared<OnnxSegmentation>(home);
}
} // namespace luma::windows
