#pragma once
#include "core/presets/SettingsSubset.h"
#include <QByteArray>
#include <QString>
#include <vector>

namespace luma {
struct DevelopPreset {
    QString name;
    QString folder;
    EditDocument settings;
    std::uint32_t groups = allSettingsGroups;
    void validate() const;
};
struct StoredPreset {
    QString id;
    DevelopPreset preset;
};
QByteArray encodePreset(const DevelopPreset &preset);
DevelopPreset decodePreset(const QByteArray &json);

// A bounded collection shared by catalogs in the same directory. Mutations reload
// under a lock and replace the complete file atomically, preserving other writers.
class PresetStore {
  public:
    explicit PresetStore(QString path) : path_(std::move(path)) {}
    std::vector<StoredPreset> load() const;
    QString put(const DevelopPreset &preset, const QString &id = {});
    void remove(const QString &id);
    static DevelopPreset importFile(const QString &path);
    static void exportFile(const QString &path, const DevelopPreset &preset);

  private:
    QString path_;
    void save(const std::vector<StoredPreset> &presets) const;
};
} // namespace luma
