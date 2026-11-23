#pragma once
#include "apps/desktop/LocalizedText.h"
#include "apps/shared/LibraryService.h"
#include "core/jobs/JobManager.h"
#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <deque>
#include <map>
#include <memory>

#ifdef _WIN32
namespace luma::windows {
class TetherControlRequests;
}
#endif

class LibraryModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QVariantList importMetadataPresets READ importMetadataPresets NOTIFY stateChanged)
    Q_PROPERTY(int count READ count NOTIFY stateChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY stateChanged)
    Q_PROPERTY(int sortOrder READ sortOrder WRITE setSortOrder NOTIFY stateChanged)
    Q_PROPERTY(QString selectedCopyName READ selectedCopyName NOTIFY stateChanged)
    Q_PROPERTY(QString selectedStackID READ selectedStackID NOTIFY stateChanged)
    Q_PROPERTY(bool viewingStack READ viewingStack NOTIFY stateChanged)
    Q_PROPERTY(bool collapseStacks READ collapseStacks WRITE setCollapseStacks NOTIFY stateChanged)
    Q_PROPERTY(bool selecting READ selecting NOTIFY stateChanged)
    Q_PROPERTY(bool working READ working NOTIFY stateChanged)
    Q_PROPERTY(bool importing READ importing NOTIFY stateChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString maintenanceStatus READ maintenanceStatus NOTIFY stateChanged)
    Q_PROPERTY(bool preparingPreviews READ preparingPreviews NOTIFY stateChanged)
    Q_PROPERTY(bool preparingDelivery READ preparingDelivery NOTIFY stateChanged)
    Q_PROPERTY(QString captureFolder READ captureFolder NOTIFY stateChanged)
    Q_PROPERTY(bool tetherAvailable READ tetherAvailable CONSTANT)
    Q_PROPERTY(bool cameraControlsAvailable READ cameraControlsAvailable CONSTANT)
    Q_PROPERTY(bool cameraBusy READ cameraBusy NOTIFY stateChanged)
    Q_PROPERTY(QVariantList tetherCameras READ tetherCameras NOTIFY stateChanged)
    Q_PROPERTY(bool cameraConnected READ cameraConnected NOTIFY stateChanged)
    Q_PROPERTY(bool cameraLiveAvailable READ cameraLiveAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool cameraLive READ cameraLive NOTIFY stateChanged)
    Q_PROPERTY(QUrl cameraFrame READ cameraFrame NOTIFY cameraFrameChanged)
    Q_PROPERTY(QVariantList cameraExposure READ cameraExposure NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString importReport READ importReport NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap lightroomReport READ lightroomReport NOTIFY stateChanged)
    Q_PROPERTY(QVariantList importSources READ importSources NOTIFY stateChanged)
    Q_PROPERTY(QVariantList duplicateReview READ duplicateReview NOTIFY stateChanged)
    Q_PROPERTY(QVariantList fingerprintRows READ fingerprintRows NOTIFY stateChanged)
    Q_PROPERTY(bool fingerprintBusy READ fingerprintBusy NOTIFY stateChanged)
    Q_PROPERTY(bool fingerprintHasNext READ fingerprintHasNext NOTIFY stateChanged)
    Q_PROPERTY(bool fingerprintHasPrevious READ fingerprintHasPrevious NOTIFY stateChanged)
    Q_PROPERTY(QString fingerprintError READ fingerprintError NOTIFY stateChanged)
    Q_PROPERTY(QVariantList collections READ collections NOTIFY stateChanged)
    Q_PROPERTY(QVariantList collectionSets READ collectionSets NOTIFY stateChanged)
    Q_PROPERTY(QVariantList catalogKeywords READ catalogKeywords NOTIFY catalogKeywordsChanged)
    Q_PROPERTY(bool semanticEnabled READ semanticEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool semanticBusy READ semanticBusy NOTIFY stateChanged)
    Q_PROPERTY(double semanticProgress READ semanticProgress NOTIFY stateChanged)
    Q_PROPERTY(QString semanticStatus READ semanticStatus NOTIFY stateChanged)
    Q_PROPERTY(QString semanticError READ semanticError NOTIFY stateChanged)
    Q_PROPERTY(QVariantList semanticResults READ semanticResults NOTIFY stateChanged)
    Q_PROPERTY(bool cullingBusy READ cullingBusy NOTIFY stateChanged)
    Q_PROPERTY(double cullingProgress READ cullingProgress NOTIFY stateChanged)
    Q_PROPERTY(QString cullingStatus READ cullingStatus NOTIFY stateChanged)
    Q_PROPERTY(QString cullingError READ cullingError NOTIFY stateChanged)
    Q_PROPERTY(QVariantList faceGroups READ faceGroups NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap cullingSummary READ cullingSummary NOTIFY stateChanged)
    Q_PROPERTY(bool cullingAutoEnabled READ cullingAutoEnabled WRITE setCullingAutoEnabled NOTIFY
                   stateChanged)
    Q_PROPERTY(QVariantMap descriptiveMetadata READ descriptiveMetadata NOTIFY metadataChanged)
    Q_PROPERTY(int backupSchedule READ backupSchedule WRITE setBackupSchedule NOTIFY stateChanged)
    Q_PROPERTY(int backupKeep READ backupKeep WRITE setBackupKeep NOTIFY stateChanged)
    Q_PROPERTY(bool autoWriteXMP READ autoWriteXMP WRITE setAutoWriteXMP NOTIFY stateChanged)
    Q_PROPERTY(QUrl backupFolder READ backupFolder CONSTANT)
  public:
    enum Role {
        AssetID = Qt::UserRole + 1,
        Filename,
        Path,
        Thumbnail,
        Rating,
        Flag,
        Label,
        Camera,
        Selected,
        Active,
        Missing,
        CullingScore,
        CullingRecommendation,
        CullingReasons,
        FaceCount,
        BurstID,
        BurstSize,
        BurstBest,
        CopyName,
        StackSize,
        StackCover
    };
    static constexpr int pageSize = 128, maximumPages = 16;
    explicit LibraryModel(std::filesystem::path catalog, QObject *parent = nullptr);
    ~LibraryModel() override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const {
        return count_;
    }
    int selectedCount() const {
        return selected_.size();
    }
    bool working() const {
        return importing_ || writing_ || closing_ || externalBusy_ || semanticBusy_ || cullingBusy_;
    }
    bool selecting() const {
        return selecting_;
    }
    bool preparingPreviews() const {
        return preparingPreviews_;
    }
    bool preparingDelivery() const {
        return preparingDelivery_;
    }
    void setExternalBusy(bool busy);
    QVariantMap descriptiveMetadata() const {
        return descriptiveMetadata_;
    }
    int backupSchedule() const {
        return backupSchedule_;
    }
    int backupKeep() const {
        return backupKeep_;
    }
    bool autoWriteXMP() const {
        return autoWriteXMP_;
    }
    void setBackupSchedule(int value);
    void setBackupKeep(int value);
    void setAutoWriteXMP(bool value);
    QUrl backupFolder() const;
    Q_INVOKABLE void loadMetadata(const QString &id);
    Q_INVOKABLE void saveDescriptiveMetadata(const QVariantMap &values);
    Q_INVOKABLE void importDescriptiveMetadata();
    Q_INVOKABLE void writeMetadataSidecar();
    Q_INVOKABLE void syncSidecars();
    Q_INVOKABLE void backupNow();
    Q_INVOKABLE void prepareOfflinePreviews();
    Q_INVOKABLE void cancelOfflinePreviews();
    Q_INVOKABLE void deliver(int format, const QString &title, const QUrl &destination,
                             const QUrl &proof);
    Q_INVOKABLE void printSelection(int format, const QString &title, const QUrl &proof);
    Q_INVOKABLE void publishWebDav(const QString &title, const QString &parent,
                                   const QString &folder, const QString &user,
                                   const QString &password, bool update);
    Q_INVOKABLE void cancelDelivery();
    Q_INVOKABLE void loadPhotoLocations();
    Q_INVOKABLE void openMapPhoto(const QString &id);
    Q_INVOKABLE QVariantList cardSources() const;
    Q_INVOKABLE void startCaptureFolder(const QUrl &folder);
    Q_INVOKABLE void stopCaptureFolder();
    Q_INVOKABLE void refreshCameras();
    Q_INVOKABLE void captureWithCamera(const QString &id, const QUrl &folder);
    Q_INVOKABLE void connectCameraControls(const QString &id);
    Q_INVOKABLE void setCameraLive(bool active);
    Q_INVOKABLE void refreshCameraExposure();
    Q_INVOKABLE void setCameraExposure(int property, qint64 value);
    Q_INVOKABLE void cancelCamera();
    bool tetherAvailable() const;
    bool cameraControlsAvailable() const;
    bool cameraBusy() const {
        return cameraBusy_;
    }
    QVariantList tetherCameras() const {
        return tetherCameras_;
    }
    bool cameraConnected() const {
        return cameraConnected_;
    }
    bool cameraLiveAvailable() const {
        return cameraLiveAvailable_;
    }
    bool cameraLive() const {
        return cameraLive_;
    }
    QUrl cameraFrame() const {
        return cameraFrame_;
    }
    QVariantList cameraExposure() const {
        return cameraExposure_;
    }
    QString captureFolder() const {
        return captureFolder_;
    }
    Q_INVOKABLE void restoreCatalog(const QUrl &backup, const QUrl &destination);
    Q_INVOKABLE void openCatalog(const QUrl &file, bool create = false);
    Q_INVOKABLE void locateOriginal(const QUrl &file);
    Q_INVOKABLE void locateFolder(const QUrl &folder, bool nearby);
    std::vector<std::string> selectionIDs() const;
    Q_INVOKABLE QStringList selectedAssetIDs() const {
        QStringList result;
        for (const auto &id : selectionIDs())
            result.append(QString::fromStdString(id));
        return result;
    }
    Q_INVOKABLE bool removeFromSelection(const QString &id);
    QString selectedCopyName() const;
    QString selectedStackID() const;
    bool viewingStack() const {
        return !filter_.stackID.empty();
    }
    bool collapseStacks() const {
        return filter_.collapseStacks;
    }
    void setCollapseStacks(bool collapse);
    Q_INVOKABLE void viewStack(const QString &id);
    Q_INVOKABLE void stackSelection();
    Q_INVOKABLE void unstackSelection();
    Q_INVOKABLE void setSelectedStackCover();
    Q_INVOKABLE void createVirtualCopy(const QString &id, const QString &name);
    Q_INVOKABLE void renameVirtualCopy(const QString &id, const QString &name);
    Q_INVOKABLE void deleteVirtualCopy(const QString &id);
    bool importing() const {
        return importing_;
    }
    bool loading() const {
        return loading_;
    }
    double progress() const {
        return progress_;
    }
    QString status() const {
        return status_.text();
    }
    QString maintenanceStatus() const {
        return maintenanceStatus_.text();
    }
    QString error() const {
        return error_;
    }
    QString importReport() const {
        return importReport_;
    }
    QVariantList importSources() const {
        return importSources_;
    }
    QVariantList duplicateReview() const {
        return duplicateReview_;
    }
    QVariantList fingerprintRows() const {
        return fingerprintRows_;
    }
    bool fingerprintBusy() const {
        return fingerprintBusy_;
    }
    bool fingerprintHasNext() const {
        return fingerprintHasNext_;
    }
    bool fingerprintHasPrevious() const {
        return fingerprintPage_ > 0;
    }
    QString fingerprintError() const {
        return fingerprintError_;
    }
    Q_INVOKABLE void openFingerprintReview();
    Q_INVOKABLE void closeFingerprintReview();
    Q_INVOKABLE void nextFingerprintPage();
    Q_INVOKABLE void previousFingerprintPage();
    QVariantList collections() const {
        return collections_;
    }
    QVariantList collectionSets() const {
        return collectionSets_;
    }
    QVariantList catalogKeywords() const {
        return catalogKeywords_;
    }
    bool semanticEnabled() const {
        return semanticEnabled_;
    }
    bool semanticBusy() const {
        return semanticBusy_;
    }
    double semanticProgress() const {
        return semanticProgress_;
    }
    QString semanticStatus() const {
        return semanticStatus_.text();
    }
    QString semanticError() const {
        return semanticError_;
    }
    QVariantList semanticResults() const {
        return semanticResults_;
    }
    Q_INVOKABLE void buildSemanticIndex();
    Q_INVOKABLE void searchSemantic(const QString &query);
    Q_INVOKABLE void cancelSemantic();
    Q_INVOKABLE void disableSemantic();
    bool cullingBusy() const {
        return cullingBusy_;
    }
    double cullingProgress() const {
        return cullingProgress_;
    }
    QString cullingStatus() const {
        return cullingStatus_.text();
    }
    QString cullingError() const {
        return cullingError_;
    }
    QVariantList faceGroups() const {
        return faceGroups_;
    }
    QVariantMap cullingSummary() const {
        return cullingSummary_;
    }
    bool cullingAutoEnabled() const {
        return cullingAutoEnabled_;
    }
    void setCullingAutoEnabled(bool enabled);
    Q_INVOKABLE void analyzeLibrary();
    Q_INVOKABLE void cancelCulling();
    Q_INVOKABLE void applyCullingSuggestions();
    Q_INVOKABLE void clearCullingIndex();
    Q_INVOKABLE void renameFaceGroup(const QString &id, const QString &name);
    Q_INVOKABLE void setIntelligentFilter(int recommendation, const QString &faceGroupID,
                                          bool bestOfBurstOnly);
    std::size_t cachedPages() const {
        return pages_.size();
    }
    void photoOpened(const QString &id);
    void refreshThumbnails(const QString &id = {});
    Q_INVOKABLE void setViewportAnchor(const QString &assetID) {
        viewportAnchor_ = assetID;
    }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void filter(const QString &search, int rating, int flag, const QString &label,
                            const QString &collection);
    int sortOrder() const {
        return static_cast<int>(filter_.order);
    }
    void setSortOrder(int order);
    Q_INVOKABLE void select(int row, bool toggle = false, bool range = false);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void activate(int row);
    Q_INVOKABLE QString reviewPhotoAt(int row) const;
    Q_INVOKABLE QStringList reviewSelection(int preferredRow) const;
    Q_INVOKABLE int reviewPhotoRow(const QString &id) const;
    Q_INVOKABLE void rate(int value);
    Q_INVOKABLE void flag(int value);
    Q_INVOKABLE void label(const QString &value);
    Q_INVOKABLE void editSmartCollection(const QString &id, const QString &name);
    Q_INVOKABLE void saveSmartCollection(const QString &id, const QString &name,
                                         const QString &revision, const QVariantMap &rule,
                                         const QString &setID = {});
    Q_INVOKABLE void createCollection(const QString &name, const QString &setID = {});
    Q_INVOKABLE void renameCollection(const QString &id, const QString &name);
    Q_INVOKABLE void updateCollection(const QString &id, const QString &name, const QString &setID);
    Q_INVOKABLE void deleteCollection(const QString &id);
    Q_INVOKABLE void saveCollectionSet(const QString &id, const QString &name,
                                       const QString &parentID);
    Q_INVOKABLE void deleteCollectionSet(const QString &id);
    Q_INVOKABLE void collectionMembership(const QString &id, bool add);
    Q_INVOKABLE void loadCatalogKeywords();
    Q_INVOKABLE void saveCatalogKeyword(const QString &previous, const QString &path,
                                        const QStringList &aliases);
    Q_INVOKABLE void deleteCatalogKeyword(const QString &path);
    Q_INVOKABLE void keywordMembership(const QString &path, bool add);
    QVariantList importMetadataPresets() const;
    Q_INVOKABLE bool saveImportMetadataPreset(const QString &name, const QVariantMap &values);
    Q_INVOKABLE bool removeImportMetadataPreset(const QString &name);
    Q_INVOKABLE bool importAdvanced(const QList<QUrl> &sources, const QVariantMap &settings,
                                    const QString &developPreset);
    QVariantMap lightroomReport() const {
        return lightroomReport_;
    }
    Q_INVOKABLE void inspectLightroom(const QUrl &source);
    Q_INVOKABLE void applyLightroom(const QUrl &source, const QString &fingerprint,
                                    const QUrl &profiles, bool rebuildMissingMasks);
    Q_INVOKABLE void importFiles(const QList<QUrl> &sources, bool copy, const QUrl &destination,
                                 bool recursive, bool skipDuplicates);
    Q_INVOKABLE bool refreshLastImport();
    Q_INVOKABLE bool refreshImportSource(const QUrl &source);
    Q_INVOKABLE bool refreshAllImportSources();
    Q_INVOKABLE void forgetImportSource(const QUrl &source);
    Q_INVOKABLE void clearLibrary();
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE bool requestClose();
  signals:
    void cameraFrameChanged();
    void stateChanged();
    void reloadStarted(bool preserveViewport);
    void viewportRestored(int row);
    void openRequested(const QUrl &url);
    void closeReady();
    void metadataChanged();
    void catalogKeywordsChanged();
    void deliveryReady(const QUrl &url);
    void printReady(const QUrl &url);
    void photoLocationsReady(const QVariantList &locations, const QString &error);
    void smartCollectionReady(const QString &id, const QString &name, const QString &revision,
                              const QVariantMap &rule);

  private:
    bool startImport(std::vector<std::filesystem::path> sources, luma::ImportOptions options);
    void runLightroom(const QUrl &source, const QString &fingerprint, const QUrl &profiles = {},
                      bool rebuildMissingMasks = false);
    bool startNextImportSourceRefresh();
    std::filesystem::path catalog_;
    luma::JobManager reads_, writes_, imports_;
    luma::JobManager semanticJobs_;
    luma::JobHandle semanticJob_;
    std::uint64_t semanticRevision_ = 0;
    bool semanticEnabled_ = false, semanticBusy_ = false;
    double semanticProgress_ = 0;
    LocalizedText semanticStatus_;
    QString semanticError_;
    QVariantList semanticResults_;
    luma::JobManager cullingJobs_;
    luma::JobHandle cullingJob_;
    std::uint64_t cullingRevision_ = 0;
    bool cullingBusy_ = false;
    bool cullingAutoEnabled_ = false;
    double cullingProgress_ = 0;
    LocalizedText cullingStatus_;
    QString cullingError_;
    QVariantList faceGroups_;
    QVariantMap cullingSummary_;
    luma::CancellationSource importCancellation_;
    // Import cancellation must not cancel later backup, metadata or relink work.
    luma::CancellationSource maintenanceCancellation_;
    luma::JobHandle countJob_, selectionJob_;
    std::map<int, luma::JobHandle> pageJobs_;
    std::map<int, std::vector<luma::LibraryItem>> pages_;
    std::deque<int> lru_;
    QSet<QString> selected_;
    QString activeID_, viewportAnchor_;
    luma::LibraryFilter filter_;
    QTimer filterTimer_;
    QVariantList collections_;
    QVariantList collectionSets_;
    QVariantList catalogKeywords_;
    std::uint64_t keywordRevision_ = 0;
    int count_ = 0, anchor_ = -1;
    std::uint64_t revision_ = 0, selectionRevision_ = 0, thumbnailGeneration_ = 0;
    bool externalBusy_ = false;
    bool preparingPreviews_ = false;
    bool preparingDelivery_ = false;
    QString captureFolder_;
    QVariantList tetherCameras_;
    bool cameraBusy_ = false;
    bool cameraConnected_ = false, cameraLiveAvailable_ = false, cameraLive_ = false;
    QUrl cameraFrame_;
    QVariantList cameraExposure_;
#ifdef _WIN32
    std::shared_ptr<luma::windows::TetherControlRequests> cameraControls_;
#endif
    void cameraRequest(const QString &id, const QUrl &folder);
    struct CaptureFile {
        qint64 size = -1, modified = 0;
        bool considered = false;
    };
    std::map<QString, CaptureFile> captureFiles_;
    QTimer captureTimer_;
    void scanCaptureFolder(bool baseline);
    bool importing_ = false, writing_ = true, closing_ = false, loading_ = false,
         selecting_ = false;
    double progress_ = 0;
    LocalizedText status_ = QStringLiteral("Opening catalog…");
    LocalizedText maintenanceStatus_;
    QString error_, importReport_;
    QVariantMap lightroomReport_;
    QVariantList importSources_, duplicateReview_;
    QVariantList fingerprintRows_;
    QString fingerprintError_;
    luma::JobHandle fingerprintJob_;
    std::uint64_t fingerprintRevision_ = 0;
    bool fingerprintOpen_ = false, fingerprintBusy_ = false, fingerprintHasNext_ = false;
    std::size_t fingerprintPage_ = 0;
    std::vector<std::optional<luma::DuplicateFingerprintCursor>> fingerprintCursors_;
    std::optional<luma::DuplicateFingerprintCursor> fingerprintLast_;
    void loadFingerprintPage(std::optional<luma::DuplicateFingerprintCursor> cursor,
                             std::size_t index);
    void invalidateFingerprintReview();
    std::deque<luma::ImportReplay> importSourceRefreshQueue_;
    int importSourceRefreshIndex_ = 0, importSourceRefreshTotal_ = 0;
    bool managedSourceRefresh_ = false;
    QVariantMap descriptiveMetadata_;
    std::uint64_t metadataRevision_ = 0;
    int backupSchedule_ = 2, backupKeep_ = 10;
    bool autoWriteXMP_ = false;
    QFileSystemWatcher sourceWatcher_;
    QTimer sourceWatchTimer_;
    luma::JobHandle sourceAvailabilityJob_;
    void initializeMaintenance();
    void analyzePendingLibrary();
    void startCulling(const std::vector<std::string> &selected, bool selectionScope);
    void watchVisibleFolders();
    void refreshSourceAvailability();
    void page(int number);
    void reload(bool clear);
    void selectionChanged();
    void selectRange(int offset, int length, bool toggle);
    void mutate(std::function<void(luma::Catalog &)> work, std::function<bool()> updateView = {},
                bool syncXMP = false);
    bool refreshMetadata(const std::vector<std::string> &ids, int role, const QVariant &value);
    void checkClose();
    void startDelivery(
        const QUrl &output,
        std::function<void(const std::filesystem::path &, const std::vector<std::string> &,
                           luma::CancellationToken, const std::function<void(double)> &)>
            publish,
        bool printing = false);
};
