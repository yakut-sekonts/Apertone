#pragma once
#include "core/export/ExportOptions.h"
#include "core/jobs/Cancellation.h"
#include "core/metadata/PhotoMetadata.h"
#include <QByteArray>
#include <iosfwd>

namespace luma::windows {
QByteArray readEmbeddedXmp(const std::filesystem::path &path);
QByteArray exportXmp(const std::filesystem::path &source, const PhotoMetadata &photo,
                     MetadataPolicy policy, const std::optional<MetadataOverlay> &overlay);
void embedXmp(const std::filesystem::path &encoded, const std::filesystem::path &staging,
              ExportFormat format, const QByteArray &packet, CancellationToken stop);
QByteArray mergeUltraHdrXmp(const QByteArray &gainMap, const QByteArray &packet);
void writeUltraHdrWithXmp(std::ostream &output, QByteArrayView jpeg, const QByteArray &packet);
void readRasterProperties(const std::filesystem::path &path, PhotoMetadata &photo,
                          DescriptiveMetadata *description = nullptr);
} // namespace luma::windows
