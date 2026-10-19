// Supplemental native lifecycle regression; no official coverage alias.
#include "apps/shared/PresetStore.h"
#include "tests/ux/NativeFinderActivation.h"
#include "tests/ux/UxContext.h"
#include "tests/ux/UxPixels.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QQuickItemGrabResult>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <sqlite3.h>
#include <tuple>

#ifndef LUMA_UX_HISTORY_READBACK
#error "This candidate requires the UX-target-only readonly history observation"
#endif

namespace ux {
namespace {
using Document = luma::EditDocument;
constexpr auto candidateID = "CANDIDATE-APP017-SLD009-FINDER-DEACTIVATION";
constexpr auto exposureID = "develop.exposureSlider";
constexpr auto contrastID = "develop.contrastSlider";
QString digest(const QString &path) {
    QFile input(path);
    require(input.open(QIODevice::ReadOnly), "Cannot read the bounded deactivation source");
    QCryptographicHash value(QCryptographicHash::Sha256);
    require(value.addData(&input), "Cannot hash complete deactivation source");
    return QString::fromLatin1(value.result().toHex());
}
QJsonObject documentJSON(const Document &value) {
    const auto encoded = QJsonDocument::fromJson(
        luma::encodePreset({"Deactivation full document", {}, value, luma::allSettingsGroups}));
    require(encoded.isObject(), "Cannot retain full document diagnostics");
    auto result = encoded.object();
    result["local_masks_serialized"] =
        QString::fromStdString(luma::encodeLocalMasks(value.localMasks));
    return result;
}
QJsonObject nativeJSON(const NativeFinderState &s) {
    return {{"nsapp_active", s.applicationActive},
            {"nsapp_hidden", s.applicationHidden},
            {"own_running_active", s.ownRunningActive},
            {"finder_active", s.finderActive},
            {"finder_terminated", s.finderTerminated},
            {"own_frontmost", s.ownFrontmost},
            {"finder_frontmost", s.finderFrontmost},
            {"window_key", s.windowKey},
            {"window_visible", s.windowVisible},
            {"window_miniaturized", s.windowMiniaturized},
            {"own_pid", s.ownPID},
            {"finder_pid", s.finderPID},
            {"native_window_number", s.windowNumber},
            {"activation_policy", s.activationPolicy},
            {"app_resigned", static_cast<int>(s.appResigned)},
            {"app_activated", static_cast<int>(s.appActivated)},
            {"window_resigned_key", static_cast<int>(s.windowResignedKey)},
            {"workspace_finder_activated", static_cast<int>(s.finderActivated)},
            {"workspace_own_activated", static_cast<int>(s.ownActivated)}};
}
bool sameMetadata(const luma::PhotoMetadata &a, const luma::PhotoMetadata &b) {
    return std::tie(a.camera, a.lens, a.iso, a.shutterSeconds, a.aperture, a.focalLength,
                    a.captureTimestamp, a.width, a.height, a.orientation) ==
           std::tie(b.camera, b.lens, b.iso, b.shutterSeconds, b.aperture, b.focalLength,
                    b.captureTimestamp, b.width, b.height, b.orientation);
}
bool sameRow(const luma::LibraryItem &a, const luma::LibraryItem &b) {
    return std::tie(a.id, a.path, a.filename, a.checksum, a.width, a.height, a.orientation,
                    a.rating, a.flag, a.label,
                    a.camera) == std::tie(b.id, b.path, b.filename, b.checksum, b.width, b.height,
                                          b.orientation, b.rating, b.flag, b.label, b.camera);
}
QJsonObject metadataJSON(const luma::PhotoMetadata &m) {
    return {
        {"camera", QString::fromStdString(m.camera)},
        {"lens", QString::fromStdString(m.lens)},
        {"iso", m.iso ? QJsonValue(*m.iso) : QJsonValue(QJsonValue::Null)},
        {"shutter_seconds",
         m.shutterSeconds ? QJsonValue(*m.shutterSeconds) : QJsonValue(QJsonValue::Null)},
        {"aperture", m.aperture ? QJsonValue(*m.aperture) : QJsonValue(QJsonValue::Null)},
        {"focal_length", m.focalLength ? QJsonValue(*m.focalLength) : QJsonValue(QJsonValue::Null)},
        {"capture_timestamp", m.captureTimestamp ? QJsonValue(QString::number(*m.captureTimestamp))
                                                 : QJsonValue(QJsonValue::Null)},
        {"width", m.width},
        {"height", m.height},
        {"orientation", m.orientation}};
}
QJsonObject rowJSON(const luma::LibraryItem &r) {
    return {{"id", QString::fromStdString(r.id)},
            {"path", QString::fromStdString(r.path.string())},
            {"filename", QString::fromStdString(r.filename)},
            {"checksum", QString::fromStdString(r.checksum)},
            {"width", r.width},
            {"height", r.height},
            {"orientation", r.orientation},
            {"rating", r.rating},
            {"flag", r.flag},
            {"label", QString::fromStdString(r.label)},
            {"camera", QString::fromStdString(r.camera)}};
}
QJsonArray readRows(Context &c, const char *query) {
    sqlite3 *opened = nullptr;
    const int result =
        sqlite3_open_v2(c.catalogFile.toUtf8().constData(), &opened, SQLITE_OPEN_READONLY, nullptr);
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(opened, sqlite3_close);
    require(result == SQLITE_OK && db, "Cannot open readonly deactivation journal reader");
    require(sqlite3_busy_timeout(db.get(), 1000) == SQLITE_OK,
            "Cannot set bounded journal read guard");
    sqlite3_stmt *prepared = nullptr;
    const int preparedResult = sqlite3_prepare_v2(db.get(), query, -1, &prepared, nullptr);
    const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(prepared,
                                                                               sqlite3_finalize);
    require(preparedResult == SQLITE_OK && statement,
            "Cannot prepare readonly journal observation");
    QJsonArray rows;
    int step = SQLITE_OK;
    while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
        QJsonArray row;
        for (int column = 0; column < sqlite3_column_count(statement.get()); ++column) {
            if (sqlite3_column_type(statement.get(), column) == SQLITE_NULL)
                row.append(QJsonValue(QJsonValue::Null));
            else
                row.append(QString::fromUtf8(
                    reinterpret_cast<const char *>(sqlite3_column_text(statement.get(), column))));
        }
        rows.append(row);
    }
    require(step == SQLITE_DONE, "Readonly journal observation did not finish");
    return rows;
}
QJsonObject journal(Context &c) {
    return {
        {"imports",
         readRows(
             c, "SELECT id,mode,destination,state,owner_pid,skip_duplicates,created_at,finished_at "
                "FROM import_sessions ORDER BY id")},
        {"items",
         readRows(c, "SELECT "
                     "session_id,position,source_path,target_path,temporary_path,checksum,filesize,"
                     "state,asset_id,error FROM import_items ORDER BY session_id,position")},
        {"session", readRows(c, "SELECT slot,asset_id FROM session ORDER BY slot")}};
}
// No fake lifecycle events. Counters observe actual Qt delivery to the real window.
class QtAudit final : public QObject {
  public:
    explicit QtAudit(Context &context) : c(context) {
        c.window->installEventFilter(this);
    }
    ~QtAudit() override {
        c.window->removeEventFilter(this);
    }
    unsigned deactivated = 0, presses = 0, releases = 0, heldMoves = 0;
    bool sawInactive = false;
    QJsonObject json() const {
        return {{"window_deactivate_events", static_cast<int>(deactivated)},
                {"presses", static_cast<int>(presses)},
                {"releases", static_cast<int>(releases)},
                {"held_moves", static_cast<int>(heldMoves)},
                {"application_state", static_cast<int>(QGuiApplication::applicationState())},
                {"saw_actual_native_and_qt_inactive", sawInactive},
                {"window_active", c.window->isActive()},
                {"window_exposed", c.window->isExposed()},
                {"history_gesture_active", c.editor.uxHistoryGestureActive()},
                {"slider_pressed", c.driver->object(exposureID)->property("pressed").toBool()}};
    }

  protected:
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::WindowDeactivate)
            ++deactivated;
        if (event->type() == QEvent::MouseButtonPress)
            ++presses;
        if (event->type() == QEvent::MouseButtonRelease)
            ++releases;
        if (event->type() == QEvent::MouseMove &&
            static_cast<QMouseEvent *>(event)->buttons().testFlag(Qt::LeftButton))
            ++heldMoves;
        return false;
    }

  private:
    Context &c;
};
// Action returns to a real nested event loop; no long QTest polling callback encloses
// the AppKit action. Dispatch alone never proves a native transition.
template <class Action, class Predicate>
void nativeTransition(Action action, Predicate predicate, int deadlineMilliseconds = 10000) {
    QEventLoop loop;
    QTimer poll, deadline;
    bool accepted = false, interrupted = false, actionReturned = false;
    std::exception_ptr failure;
    deadline.setSingleShot(true);
    const auto stopOnFailure = [&](auto operation) noexcept {
        try {
            operation();
        } catch (...) {
            failure = std::current_exception();
            loop.quit();
        }
    };
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        stopOnFailure([&] {
            if (predicate()) {
                accepted = true;
                loop.quit();
            }
        });
    });
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, &loop, [&] {
        interrupted = true;
        loop.quit();
    });
    QTimer::singleShot(0, &loop, [&] {
        stopOnFailure([&] {
            action();
            actionReturned = true;
            poll.start(16);
        });
    });
    deadline.start(deadlineMilliseconds); // Fixture liveness guard, not a product latency SLA.
    loop.exec();
    poll.stop();
    deadline.stop();
    if (failure)
        std::rethrow_exception(failure);
    require(accepted && actionReturned && !interrupted,
            "Native transition/history completion was not positively observed before cleanup");
}
struct Cleanup final {
    Context &c;
    NativeFinderActivation &native;
    bool pointerDown = false, activationAttempted = false;
    bool programmaticActivationCleanupAllowed = true;
    QPoint last{};
    ~Cleanup() noexcept {
        try {
            if (pointerDown) {
                c.driver->log({{"action", "failure_pointer_cleanup"}, {"acceptance", false}});
                QTest::mouseRelease(c.window, Qt::LeftButton, Qt::NoModifier, last);
            }
            if (activationAttempted && programmaticActivationCleanupAllowed) {
                native.requestOwnActivation();
                const bool restored = QTest::qWaitFor(
                    [&] {
                        const auto s = native.state();
                        return s.applicationActive && s.ownFrontmost && s.windowKey &&
                               c.window->isActive() &&
                               QGuiApplication::applicationState() == Qt::ApplicationActive;
                    },
                    5000);
                c.driver->log({{"action", "failure_focus_cleanup"},
                               {"restored", restored},
                               {"acceptance", false}});
            } else if (activationAttempted) {
                c.driver->log({{"action", "external_mode_programmatic_focus_cleanup_skipped"},
                               {"acceptance", false}});
            }
        } catch (...) {
            std::fputs("Native deactivation cleanup failed; original result is unchanged.\n",
                       stderr);
        }
    }
};
void exact(Context &c, const QString &id, const QString &path, const Document &expected) {
    require(c.editor.photoID() == id && c.editor.sourcePath() == path &&
                c.editor.currentSettings() == expected && c.editor.error().isEmpty() &&
                !c.editor.localMode() && !c.editor.cropMode() && !c.editor.guidedMode() &&
                !c.editor.original() && !c.editor.presetPreview() && !c.editor.editingLocked(),
            "Deactivation changed UUID/source/full document or left unsafe transient state");
}
// Exact immutable CPU reference and actual current Ready Image. Scene capture is a
// separate visual acknowledgement; shared CPU/ICC numerical correctness is out of scope.
void rendered(Context &c, const Document &expected, const QString &path, const QString &tag) {
    const auto pixels = fixtureReference(c, expected, path);
    auto *image = c.driver->item("develop.preview");
    const auto ready = [&] {
        return !c.editor.busy() && image->isVisible() && image->property("status").toInt() == 1 &&
               !c.editor.previewUrl().isEmpty() &&
               image->property("source").toUrl().toString() == c.editor.previewUrl() &&
               c.provider->requestImage({}, nullptr, {}) == pixels;
    };
    c.driver->wait(ready, "Native deactivation current Ready/exact document pixels", 10000);
    const auto url = image->property("source").toUrl();
    const auto capture = image->grabToImage();
    require(!capture.isNull(), "Actual current Image scene readback could not start");
    c.driver->wait([&] { return !capture->image().isNull(); }, "actual Image readback", 10000);
    require(ready() && image->property("source").toUrl() == url &&
                c.editor.currentSettings() == expected,
            "Preview generation changed while scene readback was pending");
    require(capture->image().save(c.root + '/' + tag + "-preview.png"),
            "Cannot save actual preview visual acknowledgement");
}
} // namespace

bool appDeactivationCandidate(Context &c, const QString &id) {
    if (id != candidateID)
        return false;
    const auto returnMode = qEnvironmentVariable("LUMA_UX_APP_RETURN_MODE");
    require(returnMode.isEmpty() || returnMode == "external-native-app-switch",
            "Unsupported supplemental-only LUMA_UX_APP_RETURN_MODE");
    const bool externalReturn = returnMode == "external-native-app-switch";
    auto &d = *c.driver;
    require(QGuiApplication::platformName() == "cocoa",
            "True cross-process deactivation is unqualified on offscreen/software-only transport");
    require(c.library.count() == 0 && !c.editor.hasPhoto(),
            "Candidate needs a fresh private Context");
    require(QFileInfo(c.fixture + "/Photo-0000.png").isFile() &&
                !QFileInfo(c.fixture + "/Photo-0000.png").isSymLink(),
            "Candidate requires the one small generated public PNG");
    d.log({{"action", "declared_import_open_setup"},
           {"native_chooser_exercised", false},
           {"physical_os_pointer", false},
           {"qt_qtest_input", true}});
    c.import(1);
    d.doubleClick(c.card(0));
    c.loaded();
    d.wait([&] { return !c.editor.saving() && !c.library.loading(); }, "neutral setup settled");
    const auto assetID = c.editor.photoID(), path = c.editor.sourcePath();
    luma::Catalog initialCatalog(c.catalogFile.toStdString());
    const auto initial = initialCatalog.assetSnapshot(assetID.toStdString());
    const auto rows = initialCatalog.libraryPage({}, 0, 2);
    const auto membership = initialCatalog.libraryIDs({});
    const auto selection = c.library.selectionIDs();
    const auto sourceSHA = digest(path);
    Document neutral, exposed = neutral;
    exposed.exposure = 5;
    require(initial && initial->asset.edits == neutral && rows.size() == 1 &&
                rows[0].id == assetID.toStdString() && membership.size() == 1 &&
                initial->asset.path == std::filesystem::path(path.toStdString()) &&
                initial->asset.checksum == sourceSHA.toStdString() &&
                initialCatalog.collections().empty() && !c.editor.canUndo() &&
                !c.editor.canRedo() && !c.editor.uxHistoryGestureActive() &&
                c.editor.currentSettings() == neutral,
            "Initial pinned one-asset/default-document/history contract is not satisfied");
    d.wait(
        [&] {
            return !c.library.loading() && c.library.count() == 1 &&
                   c.library.data(c.library.index(0), LibraryModel::AssetID).toString() ==
                       assetID &&
                   c.library.data(c.library.index(0), LibraryModel::Path).toUrl().toLocalFile() ==
                       path &&
                   c.library.data(c.library.index(0), LibraryModel::Filename).toString() ==
                       QString::fromStdString(rows[0].filename);
        },
        "actual populated Library metadata after photoOpened reload");
    const auto journalBefore = journal(c);
    QJsonArray expectedSession;
    expectedSession.append(QJsonValue(QJsonArray{QStringLiteral("1"), assetID}));
    require(journalBefore["imports"].toArray().size() == 1 &&
                journalBefore["items"].toArray().size() == 1 &&
                journalBefore["session"].toArray() == expectedSession,
            "Setup Import/session journal cardinality does not match one asset");
    const auto imported = journalBefore["imports"].toArray().at(0).toArray();
    const auto item = journalBefore["items"].toArray().at(0).toArray();
    require(imported.size() == 8 && imported[1].toString() == "add" &&
                imported[2].toString().isEmpty() && imported[3].toString() == "completed" &&
                imported[4].toString().toLongLong() > 0 && imported[5].toString() == "0" &&
                !imported[6].toString().isEmpty() && !imported[7].toString().isEmpty() &&
                item.size() == 10 && item[0] == imported[0] && item[1].toString() == "0" &&
                item[2].toString() == path && item[3].toString() == path &&
                item[4].toString().isEmpty() && item[5].toString() == sourceSHA &&
                item[6].toString().toLongLong() == QFileInfo(path).size() &&
                item[7].toString() == "committed" && item[8].toString() == assetID &&
                item[9].toString().isEmpty(),
            "Setup journal is not the exact completed one-source Add transaction");
    rendered(c, neutral, path, "baseline");
    require(fixtureReference(c, neutral, path) != fixtureReference(c, exposed, path),
            "Exposure positive control has indistinguishable pixels");
    c.reveal(exposureID);
    NativeFinderActivation native(*c.window);
    QtAudit qt(c);
    Cleanup cleanup{c, native};
    cleanup.programmaticActivationCleanupAllowed = !externalReturn;
    QSignalSpy saves(&c.editor, &EditorController::editsSaved);
    require(saves.isValid(), "Cannot observe natural durable edit completions");
    QObject handoffObserver;
    bool auditHandoff = true, observerFailed = false;
    int handoffObservations = 0, handoffInvalid = 0;
    QJsonArray handoffFailures;
    QObject::connect(&c.editor, &EditorController::stateChanged, &handoffObserver, [&]() noexcept {
        if (!auditHandoff)
            return;
        try {
            ++handoffObservations;
            if (c.editor.photoID() != assetID || c.editor.sourcePath() != path ||
                (c.editor.currentSettings() != neutral && c.editor.currentSettings() != exposed) ||
                !c.editor.error().isEmpty()) {
                ++handoffInvalid;
                if (handoffFailures.size() < 8)
                    handoffFailures.append(documentJSON(c.editor.currentSettings()));
            }
        } catch (...) {
            observerFailed = true;
        }
    });
    QJsonArray checkpoints;
    QString externalReturnNonce;
    QString phase = "native_baseline";
    const auto capture = [&](const QString &label) {
        QJsonObject value;
        // An unavailable/recreated native window must not suppress independent
        // controller/history/Qt evidence before cleanup or replace the original failure.
        try {
            value = nativeJSON(native.state());
        } catch (const std::exception &failure) {
            value["native_observation_error"] = QString::fromUtf8(failure.what());
        } catch (...) {
            value["native_observation_error"] = "Nonstandard native observation exception";
        }
        value["label"] = label;
        if (externalReturn) {
            value["return_transport"] = returnMode;
            value["external_return_nonce"] = externalReturnNonce;
        }
        try {
            value["qt"] = qt.json();
        } catch (const std::exception &failure) {
            value["qt_observation_error"] = QString::fromUtf8(failure.what());
        } catch (...) {
            value["qt_observation_error"] = "Nonstandard Qt observation exception";
        }
        value["full_document"] = documentJSON(c.editor.currentSettings());
        value["history_gesture_active"] = c.editor.uxHistoryGestureActive();
        value["uuid"] = c.editor.photoID();
        value["source"] = c.editor.sourcePath();
        value["saving"] = c.editor.saving();
        value["save_success_count"] = saves.count();
        value["handoff_state_observations"] = handoffObservations;
        value["handoff_invalid_observations"] = handoffInvalid;
        value["handoff_observer_failed"] = observerFailed;
        value["handoff_first_invalid_documents"] = handoffFailures;
        value["error"] = c.editor.error();
        try {
            value["journal"] = journal(c);
            const auto durable =
                luma::Catalog(c.catalogFile.toStdString()).assetSnapshot(assetID.toStdString());
            if (durable) {
                value["durable_revision"] = QString::number(durable->revision);
                value["durable_full_document"] = documentJSON(durable->asset.edits);
            } else {
                value["durable_missing"] = true;
            }
        } catch (const std::exception &failure) {
            value["storage_observation_error"] = QString::fromUtf8(failure.what());
        } catch (...) {
            value["storage_observation_error"] = "Nonstandard storage observation exception";
        }
        checkpoints.append(value);
    };
    const auto persist = [&](const Document &expected) {
        d.wait(
            [&] {
                const auto saved =
                    luma::Catalog(c.catalogFile.toStdString()).assetSnapshot(assetID.toStdString());
                return !c.editor.saving() && !c.editor.busy() && !c.library.loading() && saved &&
                       saved->asset.edits == expected;
            },
            "natural full-document persistence without endEdit/flush rescue", 10000);
        luma::Catalog catalog(c.catalogFile.toStdString());
        const auto current = catalog.assetSnapshot(assetID.toStdString());
        const auto currentRows = catalog.libraryPage({}, 0, 2);
        for (const auto &saved : saves)
            require(saved.size() == 1 && saved[0].toString() == assetID,
                    "A natural save success belongs to a foreign UUID");
        require(current && current->asset.edits == expected &&
                    current->revision == initial->revision + saves.count() &&
                    current->asset.path == initial->asset.path &&
                    current->asset.checksum == initial->asset.checksum &&
                    sameMetadata(current->asset.metadata, initial->asset.metadata) &&
                    currentRows.size() == 1 && sameRow(currentRows[0], rows[0]) &&
                    catalog.libraryIDs({}) == membership && catalog.collections().empty() &&
                    journal(c) == journalBefore && c.library.selectionIDs() == selection &&
                    digest(path) == sourceSHA && !c.editor.uxHistoryGestureActive(),
                "Natural persistence changed revision accounting, pinned data, membership or SHA");
        exact(c, assetID, path, expected);
    };
    try {
        const auto before = native.state();
        require(before.applicationActive && !before.applicationHidden && before.ownRunningActive &&
                    before.ownFrontmost && before.windowKey && before.windowVisible &&
                    !before.windowMiniaturized && !before.finderTerminated &&
                    QGuiApplication::applicationState() == Qt::ApplicationActive &&
                    c.window->isActive() && c.window->isExposed(),
                "Real foreground Cocoa/Qt baseline is unavailable");
        QJsonArray membershipJSON, selectionJSON;
        for (const auto &value : membership)
            membershipJSON.append(QString::fromStdString(value));
        for (const auto &value : selection)
            selectionJSON.append(QString::fromStdString(value));
        writeJson(c.root + "/app-deactivation-pinned-baseline.json",
                  {{"id", id},
                   {"uuid", assetID},
                   {"source", path},
                   {"source_sha256", sourceSHA},
                   {"source_bytes", QString::number(QFileInfo(path).size())},
                   {"durable_revision", QString::number(initial->revision)},
                   {"metadata", metadataJSON(initial->asset.metadata)},
                   {"library_row", rowJSON(rows[0])},
                   {"membership", membershipJSON},
                   {"selection", selectionJSON},
                   {"journal", journalBefore},
                   {"expected_neutral", documentJSON(neutral)},
                   {"expected_exposure5", documentJSON(exposed)},
                   {"native_baseline", nativeJSON(before)},
                   {"qt_baseline", qt.json()}});
        capture(phase);
        phase = "genuine_held_exposure";
        d.press(exposureID, d.sliderThumb(exposureID));
        cleanup.pointerDown = true;
        cleanup.last = d.point(exposureID, d.sliderThumb(exposureID));
        require(d.object(exposureID)->property("pressed").toBool() &&
                    c.editor.uxHistoryGestureActive() && c.editor.currentSettings() == neutral,
                "Actual Slider did not accept the neutral press/start a real edit gesture");
        d.move(exposureID, {1, .5});
        cleanup.last = d.point(exposureID, {1, .5});
        require(d.object(exposureID)->property("pressed").toBool() &&
                    c.editor.uxHistoryGestureActive() && qt.presses == 1 && qt.releases == 0 &&
                    qt.heldMoves >= 1 && c.editor.currentSettings() == exposed,
                "Literal full Exposure5 held-gesture positive control failed");
        capture(phase);
        auto nativeBeforeRequest = before;
        auto deactivations = qt.deactivated;
        phase = "cross_process_deactivate_before_any_release";
        cleanup.activationAttempted = true;
        nativeTransition(
            [&] {
                nativeBeforeRequest = native.state();
                deactivations = qt.deactivated;
                require(
                    nativeBeforeRequest.applicationActive && nativeBeforeRequest.ownRunningActive &&
                        nativeBeforeRequest.ownFrontmost && nativeBeforeRequest.windowKey &&
                        !nativeBeforeRequest.applicationHidden &&
                        QGuiApplication::applicationState() == Qt::ApplicationActive &&
                        c.window->isActive() &&
                        d.object(exposureID)->property("pressed").toBool() &&
                        c.editor.uxHistoryGestureActive() &&
                        c.editor.currentSettings() == exposed && qt.releases == 0,
                    "Queued action lost the genuine active, held Exposure5 gesture before handoff");
                capture("inside_action_immediately_before_native_request");
                require(native.requestFinderActivation(),
                        "AppKit rejected the actual Finder handoff");
            },
            [&] {
                const auto current = native.state();
                const bool nativeInactive =
                    !current.applicationActive && !current.ownRunningActive &&
                    current.finderActive && current.finderFrontmost && !current.finderTerminated &&
                    !current.windowKey && current.appResigned > nativeBeforeRequest.appResigned &&
                    current.finderActivated > nativeBeforeRequest.finderActivated &&
                    current.windowResignedKey > nativeBeforeRequest.windowResignedKey;
                qt.sawInactive |= nativeInactive &&
                                  QGuiApplication::applicationState() == Qt::ApplicationInactive &&
                                  !c.window->isActive() && qt.deactivated > deactivations;
                require(c.editor.currentSettings() == neutral ||
                            c.editor.currentSettings() == exposed,
                        "Deactivation produced an invalid partial/foreign full document");
                return qt.sawInactive && nativeInactive &&
                       QGuiApplication::applicationState() == Qt::ApplicationInactive &&
                       !c.window->isActive() && !c.editor.uxHistoryGestureActive() &&
                       !d.object(exposureID)->property("pressed").toBool();
            });
        // CRITICAL: this proof precedes release, reactivation, Undo and every cleanup action.
        capture(phase);
        require(!c.editor.uxHistoryGestureActive() &&
                    !d.object(exposureID)->property("pressed").toBool() && cleanup.pointerDown,
                "Gesture only appeared completed after a release/rescue action");
        const auto outcome = c.editor.currentSettings();
        const bool retained = outcome == exposed;
        require((outcome == neutral || retained) && c.editor.canUndo() == retained &&
                    !c.editor.canRedo(),
                "Automatic finalize/cancel left inconsistent exact history");
        const auto inactive = native.state();
        if (externalReturn) {
            phase = "external_native_app_switch_return";
            nativeTransition(
                [&] {
                    const auto current = native.state();
                    const auto identity = native.identity();
                    const auto executable = QFileInfo(identity.executablePath).canonicalFilePath();
                    const auto actualExecutable =
                        QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
                    require(
                        current.finderActive && current.finderFrontmost &&
                            !current.applicationActive && !current.ownRunningActive &&
                            !current.windowKey && !current.finderTerminated &&
                            QGuiApplication::applicationState() == Qt::ApplicationInactive &&
                            !c.window->isActive() && !c.editor.uxHistoryGestureActive() &&
                            !d.object(exposureID)->property("pressed").toBool() &&
                            c.editor.currentSettings() == outcome && cleanup.pointerDown,
                        "External return marker lost strict inactive/completed-control boundary");
                    require(
                        identity.ownPID == QCoreApplication::applicationPid() &&
                            identity.ownPID == current.ownPID && identity.windowNumber > 0 &&
                            identity.windowNumber == current.windowNumber &&
                            !executable.isEmpty() && executable == actualExecutable &&
                            QFileInfo(executable).isFile() && !identity.windowTitle.isEmpty() &&
                            identity.windowFrameAppKit.width() > 0 &&
                            identity.windowFrameAppKit.height() > 0,
                        "External return cannot identify the exact existing test process/window");
                    externalReturnNonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
                    const auto readyPath = c.root + "/app-deactivation-external-return-ready.json";
                    require(!QFileInfo::exists(readyPath) && !QFileInfo(readyPath).isSymLink(),
                            "External return marker must be fresh and case-owned");
                    const auto rect = [](const QRectF &value) {
                        return QJsonObject{{"x", value.x()},
                                           {"y", value.y()},
                                           {"width", value.width()},
                                           {"height", value.height()}};
                    };
                    const QJsonObject marker{
                        {"schema", 1},
                        {"id", id},
                        {"state", "ready_for_external_native_app_switch"},
                        {"nonce", externalReturnNonce},
                        {"transport", returnMode},
                        {"created_at_utc",
                         QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                        {"observation_deadline_ms", 45000},
                        {"own_pid", identity.ownPID},
                        {"canonical_executable", executable},
                        {"qt_application_file", actualExecutable},
                        {"qt_application_name", QCoreApplication::applicationName()},
                        {"native_bundle_identifier", identity.bundleIdentifier},
                        {"native_bundle_path", identity.bundlePath},
                        {"native_localized_name", identity.localizedName},
                        {"native_window_number", identity.windowNumber},
                        {"native_window_title", identity.windowTitle},
                        {"native_window_frame_appkit_points", rect(identity.windowFrameAppKit)},
                        {"native_screen_frame_appkit_points", rect(identity.screenFrameAppKit)},
                        {"coordinate_basis", "AppKit global points; CUA conversion not assumed"},
                        {"finder_pid", current.finderPID},
                        {"native_inactive", nativeJSON(current)},
                        {"qt_inactive", qt.json()},
                        {"uuid", assetID},
                        {"full_document", documentJSON(outcome)},
                        {"manual_release_issued", false},
                        {"acknowledgement_is_not_activation_proof", true},
                        {"target_dispatch_guaranteed", false}};
                    QSaveFile ready(readyPath);
                    ready.setDirectWriteFallback(false);
                    const auto bytes = QJsonDocument(marker).toJson(QJsonDocument::Indented);
                    require(ready.open(QIODevice::WriteOnly) &&
                                ready.write(bytes) == bytes.size() && ready.commit(),
                            "Cannot atomically publish own external-return marker");
                    d.log({{"action", "external_native_app_switch_ready"},
                           {"marker", readyPath},
                           {"nonce", externalReturnNonce},
                           {"activation_requested_by_test", false}});
                },
                [&] {
                    const auto current = native.state();
                    require(c.editor.currentSettings() == outcome &&
                                !c.editor.uxHistoryGestureActive() &&
                                !d.object(exposureID)->property("pressed").toBool(),
                            "External app-switch input changed the completed document/control");
                    return current.applicationActive && current.ownRunningActive &&
                           current.ownFrontmost && current.windowKey &&
                           current.appActivated > inactive.appActivated &&
                           QGuiApplication::applicationState() == Qt::ApplicationActive &&
                           c.window->isActive();
                },
                45000);
            capture("external_native_app_switch_actual_own_return_observed");
        } else {
            // Preserve the original default request and ten-second observation path.
            phase = "real_own_application_return";
            nativeTransition([&] { native.requestOwnActivation(); },
                             [&] {
                                 const auto current = native.state();
                                 return current.applicationActive && current.ownRunningActive &&
                                        current.ownFrontmost && current.windowKey &&
                                        current.appActivated > inactive.appActivated &&
                                        QGuiApplication::applicationState() ==
                                            Qt::ApplicationActive &&
                                        c.window->isActive();
                             });
        }
        cleanup.activationAttempted = false;
        require(c.editor.currentSettings() == outcome && !c.editor.uxHistoryGestureActive() &&
                    !d.object(exposureID)->property("pressed").toBool(),
                "Application return mutated the already completed/cancelled gesture");
        phase = "release_after_proven_automatic_completion";
        d.release(exposureID, {1, .5});
        cleanup.pointerDown = false;
        require(!d.object(exposureID)->property("pressed").toBool() &&
                    !c.editor.uxHistoryGestureActive() && c.editor.currentSettings() == outcome &&
                    handoffObservations > 0 && handoffInvalid == 0 && !observerFailed,
                "Release changed completed gesture or passive full-document observations failed");
        auditHandoff = false;
        persist(outcome);
        rendered(c, outcome, path, "returned");
        d.move(exposureID, {.5, .5});
        exact(c, assetID, path, outcome);
        require(!c.editor.uxHistoryGestureActive(), "Unpressed move reopened a completed gesture");
        phase = "one_clean_new_contrast_gesture";
        c.reveal(contrastID);
        auto next = outcome;
        next.contrast = 100;
        d.press(contrastID, d.sliderThumb(contrastID));
        cleanup.pointerDown = true;
        cleanup.last = d.point(contrastID, d.sliderThumb(contrastID));
        require(d.object(contrastID)->property("pressed").toBool() &&
                    c.editor.uxHistoryGestureActive(),
                "New contrast press did not start cleanly");
        d.move(contrastID, {1, .5});
        cleanup.last = d.point(contrastID, {1, .5});
        require(c.editor.currentSettings() == next, "New gesture merged with stale source fields");
        d.release(contrastID, {1, .5});
        cleanup.pointerDown = false;
        require(!d.object(contrastID)->property("pressed").toBool() &&
                    !c.editor.uxHistoryGestureActive(),
                "New gesture did not end naturally");
        persist(next);
        rendered(c, next, path, "new-gesture");
        phase = "exact_retained_or_cancelled_history";
        d.click("main.undoButton");
        persist(outcome);
        require(c.editor.canUndo() == retained && c.editor.canRedo(),
                "One new gesture did not produce exactly one separate undo entry");
        if (retained) {
            d.click("main.undoButton");
            persist(neutral);
            require(!c.editor.canUndo() && c.editor.canRedo(),
                    "Automatically retained gesture has duplicate/phantom undo entries");
            d.click("main.redoButton");
            persist(exposed);
            require(c.editor.canUndo() && c.editor.canRedo(),
                    "Old completed gesture redo is corrupt");
        }
        d.click("main.redoButton");
        persist(next);
        require(c.editor.canUndo() && !c.editor.canRedo(), "Final exact redo stack is corrupt");
        rendered(c, next, path, "final");
        capture("accepted");
        writeJson(
            c.root + "/app-deactivation-summary.json",
            {{"id", id},
             {"scope", "bounded Slider app deactivation; no minimize/physical input"},
             {"automatic_outcome", retained ? "finalized" : "cancelled"},
             {"history_completion_observed_before_release", true},
             {"native_chooser_exercised", false},
             {"physical_os_input", false},
             {"source_sha256", sourceSHA},
             {"return_transport", returnMode.isEmpty() ? "original_native_request" : returnMode},
             {"external_return_nonce", externalReturnNonce},
             {"checkpoints", checkpoints}});
        return true;
    } catch (...) {
        // Snapshot is taken before the Cleanup destructor can release/reactivate anything.
        try {
            capture("failure_before_cleanup");
            const bool unprovedControl = phase == "cross_process_deactivate_before_any_release" &&
                                         qt.sawInactive && !c.editor.uxHistoryGestureActive() &&
                                         d.object(exposureID)->property("pressed").toBool();
            writeJson(c.root + "/app-deactivation-failure.json",
                      {{"phase", phase},
                       {"checkpoints", checkpoints},
                       {"classification", unprovedControl ? "UNPROVED_WHOLE_CONTROL_STATE"
                                                          : "CANDIDATE_FAILURE_UNCLASSIFIED"},
                       {"pointer_down_before_cleanup", cleanup.pointerDown},
                       {"return_transport",
                        returnMode.isEmpty() ? "original_native_request" : returnMode},
                       {"external_return_nonce", externalReturnNonce},
                       {"source_sha256", digest(path)},
                       {"original_sha256", sourceSHA}});
        } catch (...) {
            std::fputs("Deactivation failure evidence could not be fully written.\n", stderr);
        }
        throw;
    }
}
} // namespace ux
