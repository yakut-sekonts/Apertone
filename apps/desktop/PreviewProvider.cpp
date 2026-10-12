#include "apps/desktop/PreviewProvider.h"

PreviewProvider::PreviewProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
void PreviewProvider::update(QImage image) {
    std::lock_guard lock(mutex_);
    image_ = std::move(image);
}
QImage PreviewProvider::requestImage(const QString &, QSize *size, const QSize &) {
    std::lock_guard lock(mutex_);
    if (size) {
        *size = image_.size();
    }
    return image_;
}
