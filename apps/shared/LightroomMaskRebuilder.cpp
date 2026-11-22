#include "apps/shared/LightroomMaskRebuilder.h"
#include "apps/shared/PhotoDecoder.h"
#include "core/image/Bayer.h"
#include "core/io/Path.h"
#include "platform/macos/CoreMLSky.h"
#include <QCoreApplication>
#include <QStringList>

namespace luma {
namespace {
QString field(const LightroomProperties &component, std::string_view key) {
    const auto found = component.find(key);
    return found == component.end() ? QString{} : found->second;
}

std::optional<ai::Selection> selection(const LightroomProperties &mask) {
    if (field(mask, "What") != "Mask/Image")
        return std::nullopt;
    const auto type = field(mask, "MaskSubType");
    if (type == "1")
        return ai::Selection::subject;
    if (type == "2")
        return ai::Selection::sky;
    if (type != "3")
        return std::nullopt;
    const auto category = field(mask, "MaskSubCategoryID");
    if (category == "2")
        return ai::Selection::face;
    if (category == "6")
        return ai::Selection::lips;
    return std::nullopt;
}

std::shared_ptr<ai::ISegmentationProvider> skyFallback() {
#ifdef __APPLE__
    if (QCoreApplication::instance()) {
        const auto bundle = QCoreApplication::applicationDirPath();
        for (const auto relative :
             {"/../Resources/Models/LumaSky.mlmodelc",
              "/../Apertone.app/Contents/Resources/Models/LumaSky.mlmodelc"}) {
            const auto path = pathFromUtf8((bundle + QString::fromLatin1(relative)).toStdString());
            std::error_code error;
            if (std::filesystem::is_directory(path, error))
                return macos::makeCoreMLSky(path);
        }
    }
#endif
    return {};
}
} // namespace

BitmapMask LightroomMaskRebuilder::rebuild(const LightroomProperties &component) {
    const auto kind = selection(component);
    if (!kind)
        return {};
    auto active = provider_;
    if (*kind == ai::Selection::sky && (!active || !active->supports(*kind)))
        active = skyFallback();
    if (!active || !active->supports(*kind))
        return {};
    std::array<double, 2> point{.5, .5};
    if (*kind == ai::Selection::face || *kind == ai::Selection::lips) {
        const auto position = field(component, "ReferencePoint").simplified().split(' ');
        if (position.size() != 2)
            return {};
        for (int i = 0; i < 2; ++i) {
            bool valid = false;
            point[i] = position[i].toDouble(&valid);
            if (!valid || !std::isfinite(point[i]) || point[i] < 0 || point[i] > 1)
                return {};
        }
    }
    const auto key = std::pair{*kind, point};
    if (const auto prior = cached_.find(key); prior != cached_.end())
        return prior->second;
    // Memoize misses too. A missing face is shared by its related face masks.
    auto &result = cached_[key];
    if (!image_) {
        const auto source = PhotoDecoder().decode(photo_, stop_);
        const auto input = standardPreview(source.image, 768, stop_);
        sourceWidth_ = input.width;
        sourceHeight_ = input.height;
        geometry_.emplace(input.width, input.height, source.metadata.orientation, EditDocument{});
        image_ = renderCPU(input, {}, source.metadata.orientation, {}, stop_, source.calibration);
    }
    const auto oriented =
        geometry_->output(point[0] * sourceWidth_ - .5, point[1] * sourceHeight_ - .5);
    const std::array upright{std::clamp((oriented[0] + .5) / image_->width, 0.0, 1.0),
                             std::clamp((oriented[1] + .5) / image_->height, 0.0, 1.0)};
    const ai::RgbaImage input{image_->width, image_->height, image_->rgba};
    const bool ambiguousPoint = (*kind == ai::Selection::face || *kind == ai::Selection::lips) &&
                                point == std::array{.5, .5};
    const auto mask = ambiguousPoint ? active->segmentSingleFace(input, *kind, stop_)
                                     : active->segment(input, *kind, upright, stop_);
    if (mask.empty())
        return {};
    std::vector<std::uint8_t> density(pixelCount(sourceWidth_, sourceHeight_));
    for (int y = 0; y < sourceHeight_; ++y) {
        checkCancellation(stop_);
        for (int x = 0; x < sourceWidth_; ++x) {
            const auto position = geometry_->output(x, y);
            density[static_cast<std::size_t>(y) * sourceWidth_ + x] = static_cast<std::uint8_t>(
                std::lround(255 * mask.coverage((position[0] + .5) / image_->width,
                                                (position[1] + .5) / image_->height)));
        }
    }
    return result = BitmapMask(sourceWidth_, sourceHeight_, std::move(density));
}
} // namespace luma
