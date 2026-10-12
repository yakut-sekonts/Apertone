#include "apps/desktop/ThumbnailProvider.h"
#include "core/catalog/Catalog.h"
#include "core/image/Bayer.h"
#include "core/raw/RawDecoder.h"
#include "core/render/CpuRenderer.h"
#include "platform/macos/FileHasher.h"
#include <QBuffer>
#include <QColorSpace>
#include <QImageReader>
#include <QQuickTextureFactory>
#include <QRunnable>
#include <QSaveFile>
#include <QTransform>
#include <algorithm>
#include <mutex>
#include <vector>

struct ThumbnailState {
    std::mutex mutex, cacheMutex;
    std::vector<luma::CancellationSource> active;
    bool shuttingDown = false;
};
namespace {
QImage orientImage(const QImage &input, int orientation) {
    if (orientation < 0 || orientation > 7)
        throw std::runtime_error("Invalid thumbnail orientation.");
    Qt::Orientations flips;
    if (orientation & 1)
        flips |= Qt::Horizontal;
    if (orientation & 2)
        flips |= Qt::Vertical;
    QImage image = input.flipped(flips);
    if (orientation & 4) {
        // LibRaw flip bits swap source x/y; transpose after source-axis mirrors.
        const auto output = QImage(image.height(), image.width(), QImage::Format_RGBA8888);
        QImage result = output;
        result.setColorSpace(input.colorSpace());
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                result.setPixelColor(y, x, image.pixelColor(x, y));
        return result;
    }
    return image;
}
void trimCache(const std::filesystem::path &cache) {
    std::vector<std::filesystem::directory_entry> entries;
    std::uintmax_t size = 0;
    for (const auto &entry : std::filesystem::directory_iterator(cache)) {
        if (entry.is_regular_file() && entry.path().extension() == ".jpg") {
            entries.push_back(entry);
            size += entry.file_size();
        }
    }
    constexpr std::uintmax_t budget = 512ULL * 1024 * 1024;
    if (size <= budget)
        return;
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
        return a.last_write_time() < b.last_write_time();
    });
    for (const auto &entry : entries) {
        if (size <= budget)
            break;
        size -= entry.file_size();
        std::filesystem::remove(entry.path());
    }
}
QImage thumbnail(const std::filesystem::path &catalog, const QString &id,
                 const std::shared_ptr<ThumbnailState> &state, luma::CancellationToken stop) {
    luma::checkCancellation(stop);
    luma::Catalog connection(catalog);
    const auto asset = connection.assetByID(id.toStdString());
    if (!asset)
        throw std::runtime_error("Photo is not in the catalog.");
    const auto directory =
        catalog.parent_path() / (catalog.filename().string() + ".luma-cache") / "thumbnails";
    const auto cached = directory / (asset->checksum + "-original-v1.jpg");
    {
        std::lock_guard lock(state->cacheMutex);
        QImage image(QString::fromStdString(cached.string()));
        if (!image.isNull()) {
            std::error_code error;
            std::filesystem::last_write_time(cached, std::filesystem::file_time_type::clock::now(),
                                             error);
            return image;
        }
    }
    if (luma::macos::sha256File(asset->path, stop) != asset->checksum)
        throw std::runtime_error("Original changed; thumbnail not regenerated.");
    const auto bytes = luma::RawDecoder().embeddedJPEG(asset->path, stop);
    QImage image;
    if (!bytes.empty()) {
        QByteArray encoded(reinterpret_cast<const char *>(bytes.data()),
                           static_cast<qsizetype>(bytes.size()));
        QBuffer buffer(&encoded);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "JPEG");
        const auto dimensions = reader.size();
        if (dimensions.isValid() && dimensions.width() <= 30000 && dimensions.height() <= 30000) {
            reader.setScaledSize(dimensions.scaled(384, 384, Qt::KeepAspectRatio));
            image = reader.read();
        }
        if (!image.isNull()) {
            image = orientImage(image, asset->metadata.orientation);
            if (image.colorSpace().isValid())
                image.convertToColorSpace(QColorSpace::SRgb);
            else
                image.setColorSpace(QColorSpace::SRgb);
        }
    }
    if (image.isNull()) {
        auto decoded = luma::RawDecoder().decode(asset->path, stop);
        auto small = luma::standardPreview(decoded.image, 384, stop);
        const auto pixels =
            luma::renderCPU(small, {}, decoded.metadata.orientation, {}, stop, decoded.calibration);
        image = QImage(pixels.rgba.data(), pixels.width, pixels.height, pixels.width * 4,
                       QImage::Format_RGBA8888)
                    .copy();
        image.setColorSpace(QColorSpace::SRgb);
    }
    luma::checkCancellation(stop);
    {
        std::lock_guard lock(state->cacheMutex);
        std::filesystem::create_directories(directory);
        QSaveFile file(QString::fromStdString(cached.string()));
        if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "JPEG", 85) || !file.commit())
            throw std::runtime_error("Cannot write thumbnail cache.");
        trimCache(directory);
    }
    return image;
}
class ThumbnailResponse final : public QQuickImageResponse, public QRunnable {
  public:
    ThumbnailResponse(std::filesystem::path catalog, QString id, QSize size,
                      std::shared_ptr<ThumbnailState> state)
        : catalog_(std::move(catalog)), id_(std::move(id)), size_(size), state_(std::move(state)) {
        setAutoDelete(false);
        std::lock_guard lock(state_->mutex);
        allowed_ = !state_->shuttingDown && state_->active.size() < 96;
        if (allowed_)
            state_->active.push_back(cancellation_);
    }
    QQuickTextureFactory *textureFactory() const override {
        return QQuickTextureFactory::textureFactoryForImage(image_);
    }
    QString errorString() const override {
        return error_;
    }
    void cancel() override {
        cancellation_.cancel();
    }
    void run() override {
        try {
            if (!allowed_)
                throw std::runtime_error("Thumbnail queue busy.");
            image_ = thumbnail(catalog_, id_, state_, cancellation_.token());
            if (size_.isValid())
                image_ = image_.scaled(size_, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        } catch (const std::exception &error) {
            error_ = QString::fromUtf8(error.what());
        }
        if (allowed_) {
            std::lock_guard lock(state_->mutex);
            // Sources are shared atomic tokens; remove this exact token via its assigned ID.
            auto &active = state_->active;
            const auto iterator =
                std::find_if(active.begin(), active.end(), [this](const auto &source) {
                    return source.sameState(cancellation_);
                });
            if (iterator != active.end())
                active.erase(iterator);
        }
        emit finished();
    }

  private:
    std::filesystem::path catalog_;
    QString id_, error_;
    QSize size_;
    std::shared_ptr<ThumbnailState> state_;
    luma::CancellationSource cancellation_;
    QImage image_;
    bool allowed_ = false;
};
} // namespace
ThumbnailProvider::ThumbnailProvider(std::filesystem::path catalog)
    : catalog_(std::move(catalog)), state_(std::make_shared<ThumbnailState>()) {
    pool_.setMaxThreadCount(2);
}
ThumbnailProvider::~ThumbnailProvider() {
    {
        std::lock_guard lock(state_->mutex);
        state_->shuttingDown = true;
        for (const auto &source : state_->active)
            source.cancel();
    }
    pool_.waitForDone();
}
QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id,
                                                             const QSize &requestedSize) {
    auto *response = new ThumbnailResponse(catalog_, id.section('?', 0, 0), requestedSize, state_);
    pool_.start(response);
    return response;
}
