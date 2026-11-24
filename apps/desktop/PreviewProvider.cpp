#include "apps/desktop/PreviewProvider.h"

PreviewProvider::PreviewProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
void PreviewProvider::update(QImage image) {
    std::lock_guard lock(mutex_);
    image_ = std::move(image);
}
void PreviewProvider::updateCompare(std::vector<QImage> images) {
    std::lock_guard lock(mutex_);
    comparison_ = std::move(images);
}
void PreviewProvider::updateReference(QImage image) {
    std::lock_guard lock(mutex_);
    reference_ = std::move(image);
}
QImage PreviewProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    const auto key = id.section('?', 0, 0);
    QImage result;
    {
        std::lock_guard lock(mutex_);
        if (key == QStringLiteral("reference")) {
            result = reference_;
        } else if (key.startsWith(QStringLiteral("compare-"))) {
            bool valid = false;
            const auto index = key.mid(8).toUInt(&valid);
            result = valid && index < comparison_.size() ? comparison_[index] : QImage();
        } else {
            result = image_;
        }
    }
    if (size) {
        *size = result.size();
    }
    if (key == QStringLiteral("current") && !requestedSize.isEmpty() &&
        (result.width() > requestedSize.width() || result.height() > requestedSize.height()))
        return result.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return result;
}
