#pragma once
#include <QQuickImageProvider>
#include <mutex>

class PreviewProvider final : public QQuickImageProvider {
  public:
    PreviewProvider();
    void update(QImage image);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

  private:
    std::mutex mutex_;
    QImage image_;
};
