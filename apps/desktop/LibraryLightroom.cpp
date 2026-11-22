#include "apps/desktop/LibraryModel.h"
#include "apps/shared/LightroomCatalog.h"
#include "core/io/Path.h"
#include <QFileInfo>
#include <QPointer>

namespace {
QVariantMap reportMap(const luma::LightroomCatalogReport &report, bool applied) {
    QStringList warnings;
    for (const auto &warning : report.warnings)
        warnings.append(QString::fromStdString(warning));
    const auto count = [](std::size_t value) { return QVariant::fromValue<qulonglong>(value); };
    return {
        {"fingerprint", QString::fromStdString(report.fingerprint)},
        {"photos", count(report.photos)},
        {"available", count(report.available)},
        {"missing", count(report.missing)},
        {"unsupported", count(report.unsupported)},
        {"virtualCopies", count(report.virtualCopies)},
        {"manualCollections", count(report.manualCollections)},
        {"smartCollections", count(report.smartCollections)},
        {"smartRulesMapped", count(report.smartRulesMapped)},
        {"smartCollectionsImported", count(report.imported.smartCollections)},
        {"mappedEdits", count(report.mappedEdits)},
        {"partialEdits", count(report.partialEdits)},
        {"unmappedEdits", count(report.unmappedEdits)},
        {"masksMapped", count(report.masksMapped)},
        {"masksRecovered", count(report.masksRecovered)},
        {"masksRebuilt", count(report.masksRebuilt)},
        {"masksSkipped", count(report.masksSkipped)},
        {"profilesMapped", count(report.profilesMapped)},
        {"profilesSkipped", count(report.profilesSkipped)},
        {"historyMapped", count(report.historyMapped)},
        {"historyPartial", count(report.historyPartial)},
        {"historySkipped", count(report.historySkipped)},
        {"historySteps", count(report.imported.historySteps)},
        {"snapshotsMapped", count(report.snapshotsMapped)},
        {"snapshotsPartial", count(report.snapshotsPartial)},
        {"snapshotsSkipped", count(report.snapshotsSkipped + report.imported.snapshotsWithoutRoom)},
        {"snapshots", count(report.imported.snapshots)},
        {"collectionSets", count(report.imported.collectionSets)},
        {"unreadable", count(report.unreadable)},
        {"added", count(report.imported.added)},
        {"reused", count(report.imported.reused)},
        {"versions", count(report.imported.versions)},
        {"copies", count(report.imported.copies)},
        {"unchanged", count(report.imported.unchanged)},
        {"applied", applied},
        {"warnings", warnings}};
}
} // namespace

void LibraryModel::inspectLightroom(const QUrl &source) {
    runLightroom(source, {});
}

void LibraryModel::applyLightroom(const QUrl &source, const QString &fingerprint,
                                  const QUrl &profiles, bool rebuildMissingMasks) {
    if (fingerprint.isEmpty() || fingerprint != lightroomReport_.value("fingerprint").toString())
        return;
    runLightroom(source, fingerprint, profiles, rebuildMissingMasks);
}

void LibraryModel::runLightroom(const QUrl &source, const QString &fingerprint,
                                const QUrl &profiles, bool rebuildMissingMasks) {
    if (working() || closing_ || !source.isLocalFile() ||
        QFileInfo(source.toLocalFile())
                .suffix()
                .compare(QStringLiteral("lrcat"), Qt::CaseInsensitive) != 0)
        return;
    const auto original = luma::pathFromUtf8(source.toLocalFile().toStdString());
    const auto profileFolder = luma::pathFromUtf8(profiles.toLocalFile().toStdString());
    const auto target = catalog_;
    const auto expected = fingerprint.toStdString();
    const bool apply = !expected.empty();
    importing_ = true;
    progress_ = 0;
    lightroomReport_.clear();
    error_.clear();
    status_ = apply ? QStringLiteral("Importing Lightroom catalog…")
                    : QStringLiteral("Reading Lightroom catalog…");
    importCancellation_ = {};
    const auto stop = importCancellation_.token();
    const QPointer<LibraryModel> self(this);
    emit stateChanged();
    imports_.submit(luma::JobPriority::background, [self, original, target, profileFolder, expected,
                                                    apply, rebuildMissingMasks,
                                                    stop](const auto &) {
        QVariantMap report;
        QString error;
        try {
            const auto progress = [self](double value) {
                if (!self)
                    return;
                QMetaObject::invokeMethod(
                    self,
                    [self, value] {
                        if (self && self->importing_) {
                            self->progress_ = value;
                            emit self->stateChanged();
                        }
                    },
                    Qt::QueuedConnection);
            };
            const auto result =
                apply ? luma::importLightroomCatalog(original, target, expected, stop, progress,
                                                     profileFolder, rebuildMissingMasks)
                      : luma::inspectLightroomCatalog(original, stop);
            report = reportMap(result, apply);
        } catch (const std::exception &failure) {
            if (!stop.isCancelled())
                error = QString::fromUtf8(failure.what());
        }
        if (self)
            QMetaObject::invokeMethod(
                self,
                [self, report = std::move(report), error, apply, stop] {
                    if (!self)
                        return;
                    self->importing_ = false;
                    self->lightroomReport_ = report;
                    self->error_ = error;
                    self->progress_ = report.isEmpty() ? 0 : 1;
                    self->status_ = stop.isCancelled() && report.isEmpty()
                                        ? QStringLiteral("Import cancelled")
                                    : !error.isEmpty() ? QStringLiteral("Lightroom import failed")
                                    : apply            ? QStringLiteral("Lightroom import complete")
                                            : QStringLiteral("Lightroom catalog scanned");
                    if (apply && !report.isEmpty())
                        self->reload(false);
                    emit self->stateChanged();
                    self->checkClose();
                },
                Qt::QueuedConnection);
    });
}
