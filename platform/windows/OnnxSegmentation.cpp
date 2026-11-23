#include "platform/windows/OnnxSegmentation.h"
#include "core/image/Bayer.h"
#include "core/io/Path.h"
#include "core/render/AdvancedColor.h"
#include "core/render/NoiseProfile.h"
#include <QCoreApplication>
#include <QImage>
#include <QRectF>
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
constexpr auto faceModelName = L"yunet.onnx";
constexpr auto depthModelName = L"depth-anything-v2.onnx";

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
    Ort::Env &env() {
        return env_;
    }

  private:
    struct Free {
        void operator()(HMODULE module) const {
            FreeLibrary(module);
        }
    };
    using Library = std::unique_ptr<std::remove_pointer_t<HMODULE>, Free>;
    static Library load(const std::filesystem::path &home) {
        Library library(
            LoadLibraryExW((home / L"onnxruntime.dll").c_str(), nullptr,
                           LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32));
        if (!library)
            throw std::runtime_error(
                "Cannot load ONNX Runtime. Install the x64 Microsoft Visual C++ Redistributable.");
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
Runtime &runtimeFor(const std::filesystem::path &home) {
    static Runtime runtime(home);
    return runtime;
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
        const auto resized =
            input.scaled(edge, edge, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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
        const auto *probability =
            prediction.front().GetTensorData<float>() + (channels == 2 ? count : 0);
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

class FaceModel {
  public:
    FaceModel(Runtime &runtime, const std::filesystem::path &path)
        : session_(runtime.env(), path.c_str(), options()) {
        Ort::AllocatorWithDefaultOptions allocator;
        const auto name = session_.GetInputNameAllocated(0, allocator);
        inputName_ = name.get();
        const auto type = session_.GetInputTypeInfo(0);
        const auto tensor = type.GetTensorTypeAndShapeInfo();
        if (session_.GetInputCount() != 1 || session_.GetOutputCount() != 12 ||
            tensor.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
            throw std::runtime_error("The bundled face model has incompatible tensors.");
        const auto shape = tensor.GetShape();
        if (shape != std::vector<std::int64_t>{1, 3, 640, 640})
            throw std::runtime_error("The bundled face model has incompatible dimensions.");
    }

    BitmapMask segment(ai::RgbaImage image, ai::Selection selection, std::array<double, 2> point,
                       CancellationToken stop, bool requireSingleFace = false) {
        constexpr int inputSize = 640;
        constexpr int maskSize = 512;
        if (!std::ranges::all_of(point, [](double value) {
                return std::isfinite(value) && value >= 0 && value <= 1;
            }))
            throw std::runtime_error("Invalid location for a face selection.");
        const QImage original(image.pixels.data(), image.width, image.height,
                              static_cast<qsizetype>(image.width) * 4, QImage::Format_RGBA8888);
        const auto input =
            original.scaled(inputSize, inputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (input.isNull())
            throw std::runtime_error("Cannot allocate the face detection input.");
        constexpr auto area = inputSize * inputSize;
        std::vector<float> bgr(area * 3);
        for (int y = 0; y < input.height(); ++y) {
            checkCancellation(stop);
            const auto *row = input.constScanLine(y);
            for (int x = 0; x < input.width(); ++x) {
                const auto index = y * inputSize + x;
                for (int channel = 0; channel < 3; ++channel)
                    bgr[channel * area + index] =
                        row[x * 4 + 2 - channel] * (row[x * 4 + 3] / 255.F);
            }
        }
        const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<std::int64_t, 4> shape{1, 3, inputSize, inputSize};
        const auto tensor = Ort::Value::CreateTensor<float>(memory, bgr.data(), bgr.size(),
                                                            shape.data(), shape.size());
        const char *inputs[]{inputName_.c_str()};
        constexpr std::array outputs{"cls_8",   "obj_8",  "bbox_8", "kps_8",  "cls_16",  "obj_16",
                                     "bbox_16", "kps_16", "cls_32", "obj_32", "bbox_32", "kps_32"};
        const auto prediction =
            session_.Run(Ort::RunOptions{}, inputs, &tensor, 1, outputs.data(), outputs.size());
        checkCancellation(stop);
        const QPointF click(point[0] * input.width(), point[1] * input.height());
        QRectF face;
        std::array<QPointF, 5> landmarks{};
        std::vector<QRectF> detections;
        float best = .65F;
        constexpr std::array strides{8, 16, 32};
        for (std::size_t level = 0; level < strides.size(); ++level) {
            const int stride = strides[level], grid = inputSize / stride;
            const auto output = [&](std::size_t slot, int elements) {
                const auto &value = prediction[level * 4 + slot];
                const auto info = value.GetTensorTypeAndShapeInfo();
                if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
                    info.GetElementCount() != static_cast<std::size_t>(grid * grid * elements))
                    throw std::runtime_error("The face model returned invalid output dimensions.");
                return value.GetTensorData<float>();
            };
            const auto *classes = output(0, 1), *objects = output(1, 1), *boxes = output(2, 4),
                       *points = output(3, 10);
            for (int i = 0; i < grid * grid; ++i) {
                const float score =
                    std::sqrt(std::clamp(classes[i], 0.F, 1.F) * std::clamp(objects[i], 0.F, 1.F));
                if (!std::isfinite(score) || score <= (requireSingleFace ? .65F : best))
                    continue;
                const auto *box = boxes + i * 4;
                const auto *coordinates = points + i * 10;
                if (!std::all_of(box, box + 4, [](float x) { return std::isfinite(x); }) ||
                    !std::all_of(coordinates, coordinates + 10,
                                 [](float x) { return std::isfinite(x) && std::abs(x) < 100; }))
                    continue;
                const int column = i % grid, row = i / grid;
                const double w = std::exp(std::clamp(box[2], -5.F, 5.F)) * stride;
                const double h = std::exp(std::clamp(box[3], -5.F, 5.F)) * stride;
                QRectF candidate((column + box[0]) * stride - w / 2,
                                 (row + box[1]) * stride - h / 2, w, h);
                if ((!requireSingleFace && !candidate.contains(click)) || w < 8 || h < 8 ||
                    w > inputSize * 1.5 || h > inputSize * 1.5)
                    continue;
                if (requireSingleFace)
                    detections.push_back(candidate);
                if (score <= best)
                    continue;
                face = candidate;
                best = score;
                for (std::size_t p = 0; p < landmarks.size(); ++p)
                    landmarks[p] = {(column + coordinates[p * 2]) * stride,
                                    (row + coordinates[p * 2 + 1]) * stride};
            }
        }
        if (face.isEmpty())
            throw std::runtime_error("No face was detected at that point.");
        if (requireSingleFace && std::ranges::any_of(detections, [&](const QRectF &candidate) {
                const auto overlap = face.intersected(candidate);
                const double intersection = overlap.width() * overlap.height();
                const double area = face.width() * face.height() +
                                    candidate.width() * candidate.height() - intersection;
                return intersection / area < .4 && !face.contains(candidate.center()) &&
                       !candidate.contains(face.center());
            }))
            throw std::runtime_error("The original does not identify a single face.");
        const auto eyeLine = landmarks[1] - landmarks[0];
        const double angle = std::atan2(eyeLine.y(), eyeLine.x());
        const double cosine = std::cos(angle), sine = std::sin(angle);
        const auto ellipse = [cosine, sine](QPointF p, QPointF c, double rx, double ry) {
            const auto d = p - c;
            const double x = (d.x() * cosine + d.y() * sine) / rx;
            const double y = (-d.x() * sine + d.y() * cosine) / ry;
            const double a = std::clamp((1 - x * x - y * y) * 3, 0.0, 1.0);
            return a * a * (3 - 2 * a);
        };
        const auto mouth = (landmarks[3] + landmarks[4]) * .5;
        const auto corners = landmarks[3] - landmarks[4];
        const auto mouthWidth = std::clamp(std::hypot(corners.x(), corners.y()) * .68,
                                           face.width() * .10, face.width() * .34);
        std::vector<std::uint8_t> mask(maskSize * maskSize);
        for (int y = 0; y < maskSize; ++y) {
            checkCancellation(stop);
            for (int x = 0; x < maskSize; ++x) {
                const QPointF p((x + .5) * input.width() / maskSize,
                                (y + .5) * input.height() / maskSize);
                const double coverage =
                    selection == ai::Selection::face
                        ? ellipse(p, face.center(), face.width() * .49, face.height() * .52)
                    : selection == ai::Selection::eyes
                        ? std::max(
                              ellipse(p, landmarks[0], face.width() * .115, face.height() * .067),
                              ellipse(p, landmarks[1], face.width() * .115, face.height() * .067))
                        : ellipse(p, mouth, mouthWidth, face.height() * .082);
                mask[y * maskSize + x] = static_cast<std::uint8_t>(std::lround(coverage * 255));
            }
        }
        if (*std::ranges::max_element(mask) < 128)
            throw std::runtime_error("The face is too small for a usable detail mask.");
        return BitmapMask(maskSize, maskSize, std::move(mask));
    }

  private:
    Ort::Session session_;
    std::string inputName_;
};

bool faceSelection(ai::Selection selection) {
    return selection == ai::Selection::face || selection == ai::Selection::eyes ||
           selection == ai::Selection::lips;
}

class DepthModel {
  public:
    DepthModel(Runtime &runtime, const std::filesystem::path &path)
        : session_(runtime.env(), path.c_str(), options()) {
        const auto type = session_.GetInputTypeInfo(0);
        const auto info = type.GetTensorTypeAndShapeInfo();
        const auto shape = info.GetShape();
        if (session_.GetInputCount() != 1 || session_.GetOutputCount() != 1 ||
            info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || shape.size() != 4 ||
            (shape[0] != 1 && shape[0] != -1) || shape[1] != 3 || shape[2] != -1 || shape[3] != -1)
            throw std::runtime_error("The bundled depth model has incompatible input dimensions.");
    }
    BitmapMask segment(ai::RgbaImage image, const std::array<double, 2> &range,
                       CancellationToken stop) {
        const auto [lo, hi] = range;
        if (!std::isfinite(lo) || !std::isfinite(hi) || lo < 0 || hi > 1 || hi - lo < .05)
            throw std::runtime_error("Choose a depth range at least five percent wide.");
        const QImage original(image.pixels.data(), image.width, image.height,
                              static_cast<qsizetype>(image.width) * 4, QImage::Format_RGBA8888);
        const auto fit = original.size().scaled(edge, edge, Qt::KeepAspectRatio);
        const auto align = [](int value) { return std::max(14, ((value + 7) / 14) * 14); };
        const auto input = original.scaled(align(fit.width()), align(fit.height()),
                                           Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (input.isNull())
            throw std::runtime_error("Cannot allocate a depth model input.");
        const auto width = input.width(), height = input.height();
        constexpr float mean[]{.485F, .456F, .406F}, deviation[]{.229F, .224F, .225F};
        const auto count = pixelCount(width, height);
        std::vector<float> rgb(count * 3);
        for (int y = 0; y < height; ++y) {
            checkCancellation(stop);
            const auto *row = input.constScanLine(y);
            for (int x = 0; x < width; ++x) {
                const auto *pixel = row + 4 * x;
                for (int c = 0; c < 3; ++c)
                    rgb[c * count + y * width + x] =
                        (pixel[c] / 255.F * (pixel[3] / 255.F) - mean[c]) / deviation[c];
            }
        }
        const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<std::int64_t, 4> shape{1, 3, height, width};
        const auto tensor = Ort::Value::CreateTensor<float>(memory, rgb.data(), rgb.size(),
                                                            shape.data(), shape.size());
        const char *inputs[]{"pixel_values"}, *outputs[]{"predicted_depth"};
        const auto prediction = session_.Run(Ort::RunOptions{}, inputs, &tensor, 1, outputs, 1);
        checkCancellation(stop);
        const auto info = prediction.front().GetTensorTypeAndShapeInfo();
        if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
            info.GetShape() != std::vector<std::int64_t>{1, height, width})
            throw std::runtime_error("The depth model returned incompatible dimensions.");
        const auto *plane = prediction.front().GetTensorData<float>();
        const auto depth = std::span(plane, count);
        if (!std::ranges::all_of(depth, [](float d) { return std::isfinite(d); }))
            throw std::runtime_error("The local model returned invalid depths.");
        std::vector<float> ranked(depth.begin(), depth.end());
        const auto upper = ranked.end() - 1 - ranked.size() / 20;
        std::nth_element(ranked.begin(), upper, ranked.end());
        const auto maximum = *upper;
        const auto lower = ranked.begin() + ranked.size() / 20;
        std::nth_element(ranked.begin(), lower, upper);
        const auto minimum = *lower;
        if (maximum - minimum < 1e-5F)
            throw std::runtime_error("The photo has no distinguishable depth range.");
        std::vector<std::uint8_t> result(depth.size());
        const auto step = [](double x) {
            x = std::clamp(x, 0.0, 1.0);
            return x * x * (3 - 2 * x);
        };
        for (std::size_t i = 0; i < depth.size(); ++i) {
            const double value = std::clamp((depth[i] - minimum) / (maximum - minimum), 0.F, 1.F);
            const double low = lo <= 0 ? 1 : step((value - lo + .04) / .04);
            const double high = hi >= 1 ? 1 : step((hi - value + .04) / .04);
            result[i] = static_cast<std::uint8_t>(std::lround(low * high * 255));
        }
        return BitmapMask(input.width(), input.height(), std::move(result));
    }

  private:
    static constexpr int edge = 518;
    Ort::Session session_;
};

class OnnxDenoise final : public ai::IDenoiseProvider {
  public:
    OnnxDenoise(Runtime &runtime, const std::filesystem::path &path)
        : session_(runtime.env(), path.c_str(), options()) {
        if (session_.GetInputCount() != 1 || session_.GetOutputCount() != 1)
            throw std::runtime_error("The bundled denoise model has incompatible tensors.");
    }
    std::string identifier() const override {
        return "drunet-color-onnx:2ae3ab5eb15d";
    }
    ai::ExecutionLocation location() const override {
        return ai::ExecutionLocation::local;
    }
    FloatImage denoise(const FloatImage &linear, double amount, CancellationToken stop,
                       const ai::Progress &progress) override {
        if (!std::isfinite(amount) || amount < 0 || amount > 1 || !linear.alpha.empty())
            throw std::runtime_error("Denoise requires opaque image pixels and a valid strength.");
        const auto estimate = estimateNoiseProfile(linear, {}, stop);
        const float sigma = static_cast<float>(
            std::clamp(4 * std::max(estimate.measuredLuminance, estimate.measuredColor), .01, .12));
        FloatImage result = linear;
        constexpr int halo = 64, step = 384;
        const auto reflect = [](int value, int bound) {
            if (bound == 1)
                return 0;
            const int period = 2 * (bound - 1);
            value = (value % period + period) % period;
            return value < bound ? value : period - value;
        };
        const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const char *inputs[]{"input"}, *outputs[]{"output"};
        const auto tiles = ((linear.width + step - 1) / step) * ((linear.height + step - 1) / step);
        int done = 0;
        for (int y0 = 0; y0 < linear.height; y0 += step) {
            for (int x0 = 0; x0 < linear.width; x0 += step) {
                checkCancellation(stop);
                const int width = std::min(step, linear.width - x0),
                          height = std::min(step, linear.height - y0);
                const int nx = ((width + 2 * halo + 7) / 8) * 8,
                          ny = ((height + 2 * halo + 7) / 8) * 8;
                const auto count = pixelCount(nx, ny);
                std::vector<float> rgb(count * 4, sigma);
                for (int y = 0; y < ny; ++y) {
                    checkCancellation(stop);
                    const auto row =
                        static_cast<std::size_t>(reflect(y0 + y - halo, linear.height)) *
                        linear.width;
                    for (int x = 0; x < nx; ++x) {
                        const auto at = (row + reflect(x0 + x - halo, linear.width)) * 3;
                        for (int c = 0; c < 3; ++c)
                            rgb[c * count + y * nx + x] = static_cast<float>(
                                encodeSRGB(std::clamp(linear.rgb[at + c], 0.F, 1.F)));
                    }
                }
                const std::array<std::int64_t, 4> shape{1, 4, ny, nx};
                const auto tensor = Ort::Value::CreateTensor<float>(memory, rgb.data(), rgb.size(),
                                                                    shape.data(), shape.size());
                const auto prediction =
                    session_.Run(Ort::RunOptions{}, inputs, &tensor, 1, outputs, 1);
                checkCancellation(stop);
                const auto info = prediction.front().GetTensorTypeAndShapeInfo();
                if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
                    info.GetShape() != std::vector<std::int64_t>{1, 3, ny, nx})
                    throw std::runtime_error("The denoise model returned incompatible dimensions.");
                const auto *output = prediction.front().GetTensorData<float>();
                for (int y = 0; y < height; ++y) {
                    checkCancellation(stop);
                    for (int x = 0; x < width; ++x) {
                        const auto at =
                            (static_cast<std::size_t>(y0 + y) * linear.width + x0 + x) * 3;
                        const auto original = std::span(linear.rgb).subspan(at, 3);
                        if (!std::ranges::all_of(original,
                                                 [](float n) { return n >= 0 && n <= 1; }))
                            continue;
                        const auto index = (y + halo) * nx + x + halo;
                        for (int c = 0; c < 3; ++c) {
                            const auto value = output[c * count + index];
                            if (!std::isfinite(value))
                                throw std::runtime_error(
                                    "The denoise model returned invalid pixels.");
                            const auto filtered =
                                static_cast<float>(decodeSRGB(std::clamp(value, 0.F, 1.F)));
                            result.rgb[at + c] =
                                std::lerp(original[c], filtered, static_cast<float>(amount));
                        }
                    }
                }
                ++done;
                if (progress)
                    progress(double(done) / tiles);
            }
        }
        return result;
    }

  private:
    Ort::Session session_;
};

void keepComponent(std::vector<std::uint8_t> &mask, int edge, std::array<double, 2> point,
                   CancellationToken stop) {
    if (!std::isfinite(point[0]) || !std::isfinite(point[1]) || point[0] < 0 || point[1] < 0 ||
        point[0] > 1 || point[1] > 1)
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
        if (index % edge > 0)
            visit(index - 1);
        if (index % edge < edge - 1)
            visit(index + 1);
        if (index >= edge)
            visit(index - edge);
        if (index < edge * (edge - 1))
            visit(index + edge);
    }
    mask.swap(selected);
}

class OnnxSegmentation final : public ai::ISegmentationProvider {
  public:
    explicit OnnxSegmentation(std::filesystem::path home) : home_(std::move(home)) {}
    std::string identifier() const override {
        return "onnx.windows.segmentation.v2";
    }
    ai::ExecutionLocation location() const override {
        return ai::ExecutionLocation::local;
    }
    bool supports(ai::Selection selection) const override {
        const auto index = modelIndex(selection);
        std::error_code error;
        if (faceSelection(selection))
            return std::filesystem::is_regular_file(home_ / L"Models" / faceModelName, error);
        if (selection == ai::Selection::depthRange)
            return std::filesystem::is_regular_file(home_ / L"Models" / depthModelName, error);
        return index &&
               std::filesystem::is_regular_file(home_ / L"Models" / modelNames[*index], error);
    }
    BitmapMask segmentSingleFace(ai::RgbaImage image, ai::Selection selection,
                                 CancellationToken stop) override {
        image.validate();
        checkCancellation(stop);
        if (!faceSelection(selection) || !supports(selection))
            return {};
        return getFaceModel().segment(image, selection, {.5, .5}, stop, true);
    }
    BitmapMask segment(ai::RgbaImage image, ai::Selection selection, std::array<double, 2> point,
                       CancellationToken stop, const ai::Progress &progress) override {
        image.validate();
        checkCancellation(stop);
        if (!supports(selection))
            throw std::runtime_error("The requested AI selection is not available locally.");
        if (progress)
            progress(.1);
        if (faceSelection(selection)) {
            auto mask = getFaceModel().segment(image, selection, point, stop);
            if (progress)
                progress(1);
            return mask;
        }
        if (selection == ai::Selection::depthRange) {
            auto mask = getDepthModel().segment(image, point, stop);
            if (progress)
                progress(1);
            return mask;
        }
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

    std::array<float, 2> categoryScores(ai::RgbaImage image, CancellationToken stop) {
        const auto score = [&](ai::Selection category) {
            const auto mask = getModel(*modelIndex(category)).infer(image, stop);
            const auto confidentPixels =
                std::ranges::count_if(mask, [](std::uint8_t value) { return value >= 192; });
            const auto coverage =
                static_cast<float>(confidentPixels) / static_cast<float>(mask.size());
            // Favor a confident region over isolated predictions while letting
            // small regions compete with those which fill most of the frame.
            return .7F * coverage / (coverage + .04F);
        };
        return {score(ai::Selection::people), score(ai::Selection::sky)};
    }

  private:
    static std::optional<std::size_t> modelIndex(ai::Selection selection) {
        switch (selection) {
        case ai::Selection::subject:
        case ai::Selection::background:
        case ai::Selection::object:
            return 0;
        case ai::Selection::sky:
            return 1;
        case ai::Selection::people:
            return 2;
        default:
            return std::nullopt;
        }
    }
    FaceModel &getFaceModel() {
        std::lock_guard lock(mutex_);
        if (!face_)
            face_ =
                std::make_unique<FaceModel>(runtimeFor(home_), home_ / L"Models" / faceModelName);
        return *face_;
    }
    DepthModel &getDepthModel() {
        std::lock_guard lock(mutex_);
        if (!depth_)
            depth_ =
                std::make_unique<DepthModel>(runtimeFor(home_), home_ / L"Models" / depthModelName);
        return *depth_;
    }
    Model &getModel(std::size_t index) {
        std::lock_guard lock(mutex_);
        if (!models_[index])
            models_[index] =
                std::make_unique<Model>(runtimeFor(home_), home_ / L"Models" / modelNames[index],
                                        index == 2 ? 192 : 320, index == 2 ? 2 : 1);
        return *models_[index];
    }
    std::filesystem::path home_;
    std::mutex mutex_;
    std::array<std::unique_ptr<Model>, modelNames.size()> models_;
    std::unique_ptr<FaceModel> face_;
    std::unique_ptr<DepthModel> depth_;
};

class OnnxCategoryEmbedding final : public ai::IEmbeddingProvider {
  public:
    explicit OnnxCategoryEmbedding(const std::filesystem::path &home) : segmentation_(home) {}
    std::string identifier() const override {
        return "apertone.semantic-windows-v1.pphumanseg.skyseg";
    }
    ai::ExecutionLocation location() const override {
        return ai::ExecutionLocation::local;
    }
    bool available() const {
        return segmentation_.supports(ai::Selection::people) &&
               segmentation_.supports(ai::Selection::sky);
    }
    std::vector<float> imageEmbedding(ai::RgbaImage image, CancellationToken stop) override {
        image.validate();
        checkCancellation(stop);
        const auto [people, sky] = segmentation_.categoryScores(image, stop);
        checkCancellation(stop);
        // Keep the vector unit length so a second recognized category does not
        // change the cosine score of the first category.
        return {people, sky, std::sqrt(1.F - people * people - sky * sky)};
    }
    std::vector<float> textEmbedding(const std::string &query, CancellationToken stop) override {
        checkCancellation(stop);
        if (query == "people")
            return {1, 0, 0};
        if (query == "sky")
            return {0, 1, 0};
        throw std::runtime_error("Choose a supported search category.");
    }

  private:
    OnnxSegmentation segmentation_;
};
} // namespace

std::shared_ptr<ai::ISegmentationProvider> makeOnnxSegmentation() {
    const auto home = modelHome();
    return home.empty() ? nullptr : std::make_shared<OnnxSegmentation>(home);
}
std::shared_ptr<ai::IEmbeddingProvider> makeOnnxCategoryEmbedding() {
    const auto home = modelHome();
    if (home.empty())
        return {};
    const auto provider = std::make_shared<OnnxCategoryEmbedding>(home);
    return provider->available() ? provider : nullptr;
}
std::shared_ptr<ai::IDenoiseProvider> makeOnnxDenoise(const std::filesystem::path &model) {
    const auto home = modelHome();
    if (home.empty() || model != home / L"Models" / L"drunet-color.onnx")
        return {};
    return std::make_shared<OnnxDenoise>(runtimeFor(home), model);
}
} // namespace luma::windows
