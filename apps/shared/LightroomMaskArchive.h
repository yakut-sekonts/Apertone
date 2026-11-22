#pragma once
#include "apps/shared/LightroomDevelop.h"
#include "core/edits/BitmapMask.h"
#include "core/edits/LocalMasks.h"
#include "core/jobs/Cancellation.h"
#include <QByteArray>
#include <QString>
#include <filesystem>
#include <functional>
#include <map>

namespace luma {
// Reads content-addressed grayscale data from the companion Classic catalog.
// The database and its cache files are opened for reading only.
class LightroomMaskArchive {
  public:
    LightroomMaskArchive(const std::filesystem::path &catalog, CancellationToken stop);
    BitmapMask bitmap(const QString &digest, const QString &origin, const QString &area,
                      CancellationToken stop) const;
    bool contains(const QString &digest) const;

  private:
    struct Payload {
        QString path;
        qint64 offset = 0;
        qint64 bytes = 0;
        QByteArray inlineValue;
    };
    void indexBlob(const QString &path, CancellationToken stop);
    void indexInline(const QByteArray &data, const QString &file, CancellationToken stop);
    void indexLog(const QString &path, CancellationToken stop);
    std::map<QByteArray, Payload> masks_;
    std::size_t inlineBytes_ = 0;
};
LocalMask
convertLightroomMask(const LightroomMaskGroup &group, const LightroomMaskArchive &archive,
                     std::size_t index, CancellationToken stop,
                     const std::function<BitmapMask(const LightroomProperties &)> &fallback = {});
} // namespace luma
