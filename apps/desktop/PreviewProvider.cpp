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
QImage PreviewProvider::requestImage(const QString &id, QSize *size, const QSize &) {
    std::lock_guard lock(mutex_);
    const auto key = id.section('?', 0, 0);
    QImage result = image_;
    if (key.startsWith(QStringLiteral("compare-"))) {
        bool valid = false;
        const auto index = key.mid(8).toUInt(&valid);
        result = valid && index < comparison_.size() ? comparison_[index] : QImage();
    }
    if (size) {
        *size = result.size();
    }
    return result;
}
