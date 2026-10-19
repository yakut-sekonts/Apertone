#pragma once
#include <QQuickAsyncImageProvider>
#include <QThreadPool>
#include <filesystem>
#include <memory>

#if defined(LUMA_UX_THUMBNAIL_TRACE)
namespace ux::thumbnail_trace {
class Slot;
}
#endif
struct ThumbnailState;
class ThumbnailProvider final : public QQuickAsyncImageProvider {
  public:
    explicit ThumbnailProvider(std::filesystem::path catalog);
    ~ThumbnailProvider() override;
    QQuickImageResponse *requestImageResponse(const QString &id,
                                              const QSize &requestedSize) override;
#if defined(LUMA_UX_THUMBNAIL_TRACE)
    std::shared_ptr<ux::thumbnail_trace::Slot> uxTraceSlot() const noexcept {
        return uxTraceSlot_;
    }
#endif

  private:
    std::filesystem::path catalog_;
    QThreadPool pool_;
    std::shared_ptr<ThumbnailState> state_;
#if defined(LUMA_UX_THUMBNAIL_TRACE)
    std::shared_ptr<ux::thumbnail_trace::Slot> uxTraceSlot_;
#endif
};
