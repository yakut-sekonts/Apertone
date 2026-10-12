#pragma once
#include <QQuickAsyncImageProvider>
#include <QThreadPool>
#include <filesystem>
#include <memory>

struct ThumbnailState;
class ThumbnailProvider final : public QQuickAsyncImageProvider {
  public:
    explicit ThumbnailProvider(std::filesystem::path catalog);
    ~ThumbnailProvider() override;
    QQuickImageResponse *requestImageResponse(const QString &id,
                                              const QSize &requestedSize) override;

  private:
    std::filesystem::path catalog_;
    QThreadPool pool_;
    std::shared_ptr<ThumbnailState> state_;
};
