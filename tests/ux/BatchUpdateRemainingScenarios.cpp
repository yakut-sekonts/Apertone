#include "tests/ux/QueuedCallGate.h"
#include "tests/ux/UpdaterProbe.h"
#include "tests/ux/UxContext.h"
#include "tests/ux/UxPixels.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPointer>
#include <QSignalSpy>
#include <QStyleHints>
#include <QTest>
#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <stdexcept>

namespace ux {
namespace {
using Document = luma::EditDocument;
using Documents = std::map<std::string, Document>;
using Revisions = std::map<std::string, std::int64_t>;
struct ReleaseWrites {
    SqliteFault &fault;
    ~ReleaseWrites() {
        fault.holdWrites(false);
    }
};
struct KeyboardNavigation {
    Qt::TabFocusBehavior previous = QGuiApplication::styleHints()->tabFocusBehavior();
    KeyboardNavigation() {
        QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
    }
    ~KeyboardNavigation() {
        QGuiApplication::styleHints()->setTabFocusBehavior(previous);
    }
};
struct ReleasePointer {
    Context &context;
    QPoint last;
    bool down = false;
    ~ReleasePointer() {
        // Failure cleanup only; the acceptance release still uses Driver below.
        if (down)
            QTest::mouseRelease(context.window, Qt::LeftButton, Qt::NoModifier, last);
    }
};
struct Database {
    sqlite3 *handle = nullptr;
    explicit Database(const QString &path) {
        const int result =
            sqlite3_open_v2(path.toUtf8().constData(), &handle, SQLITE_OPEN_READWRITE, nullptr);
        if (result != SQLITE_OK) {
            if (handle)
                sqlite3_close(handle);
            handle = nullptr;
            require(false, "Cannot open isolated batch/update evidence database");
        }
        // Opening the WAL while background Catalog readers open/close can return
        // BUSY (including BUSY_RECOVERY) before the first read. Use the same
        // bounded wait as Catalog; evidence queries must not fail immediately.
        sqlite3_busy_timeout(handle, 3000);
    }
    ~Database() {
        if (handle)
            sqlite3_close(handle);
    }
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    void execute(const char *sql) {
        char *raw = nullptr;
        const int code = sqlite3_exec(handle, sql, nullptr, nullptr, &raw);
        const QString message = raw ? QString::fromUtf8(raw) : QString{};
        sqlite3_free(raw);
        require(code == SQLITE_OK, "Batch/update fixture SQL failed: " + message);
    }
    std::int64_t scalar(const QByteArray &sql) const {
        sqlite3_stmt *raw = nullptr;
        const int prepared = sqlite3_prepare_v2(handle, sql.constData(), -1, &raw, nullptr);
        const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(
            raw, sqlite3_finalize);
        require(prepared == SQLITE_OK,
                "Cannot prepare batch footprint query [" + QString::fromUtf8(sql) +
                    "]: code=" + QString::number(prepared) +
                    " extended=" + QString::number(sqlite3_extended_errcode(handle)) + " " +
                    QString::fromUtf8(sqlite3_errmsg(handle)));
        const int stepped = sqlite3_step(raw);
        require(stepped == SQLITE_ROW && sqlite3_column_type(raw, 0) == SQLITE_INTEGER,
                "Footprint query did not return an integer [" + QString::fromUtf8(sql) +
                    "]: code=" + QString::number(stepped) +
                    " extended=" + QString::number(sqlite3_extended_errcode(handle)) + " " +
                    QString::fromUtf8(sqlite3_errmsg(handle)));
        return sqlite3_column_int64(raw, 0);
    }
};
Documents documents(Context &c) {
    luma::Catalog catalog(c.catalogFile.toStdString());
    Documents result;
    for (const auto &id : catalog.libraryIDs({})) {
        const auto asset = catalog.assetByID(id);
        if (!asset)
            throw std::runtime_error("Catalog document snapshot lost a fixture asset");
        result.emplace(id, asset->edits);
    }
    return result;
}
Revisions revisions(Context &c) {
    luma::Catalog catalog(c.catalogFile.toStdString());
    Revisions result;
    for (const auto &id : catalog.libraryIDs({})) {
        const auto asset = catalog.assetSnapshot(id);
        if (!asset)
            throw std::runtime_error("Catalog revision snapshot lost a fixture asset");
        result.emplace(id, asset->revision);
    }
    return result;
}
void selected(Context &c, int count) {
    auto &d = *c.driver;
    if (!c.window->property("libraryMode").toBool())
        d.click("main.libraryButton");
    d.click("library.selectAll");
    d.wait([&] { return !c.library.selecting() && c.library.selectedCount() == count; },
           "Exact batch selection acknowledged");
}
void batchIdle(Context &c) {
    c.driver->wait(
        [&] { return !c.batch.working() && !c.library.loading() && !c.library.working(); },
        "Batch worker/Library notification completion", 60000);
    c.driver->wait([&] { return !c.driver->object("batch.wait")->property("visible").toBool(); },
                   "Batch modal fully closed");
    require(!c.editor.editingLocked(), "Settled batch retained editor lock");
}
Documents expectedPreset(Context &c, const Documents &before) {
    c.driver->choose("batchPresetChoice", 1);
    const auto items = c.presets.builtinItems();
    require(items.size() > 1 && items[1].toMap()["id"].toString() == "builtin:soft-portrait" &&
                c.driver->object("batchPresetChoice")->property("currentIndex").toInt() == 1,
            "Batch fixture requires the actual Soft Portrait UI choice");
    // Independent fixed input contract. Do not invoke mergeSettings or copy a
    // computed preset result: applyBatchEdits uses that same merge implementation.
    const std::array<luma::HSLBand, 8> hsl{
        {{0, -8, 2}, {0, -6, 4}, {0, -10, 0}, {0, -12, 0}, {0, -8, 0}, {0, -8, 0}, {}, {}}};
    constexpr std::uint32_t curveAndHsl = 512U | 1024U;
    Documents expected;
    for (const auto &[photo, doc] : before) {
        auto next = doc;
        next.curveShadows = next.curveDarks = next.curveLights = next.curveHighlights = 0;
        next.curves = {};
        next.curves[0].points = {{0, .015}, {.2, .22}, {.5, .53}, {.8, .81}, {1, .98}};
        next.hsl = hsl;
        next.enabledCorrections = (doc.enabledCorrections & ~curveAndHsl) | curveAndHsl;
        require(next != doc, "Every selected fixture must change under the fixed batch input");
        expected.emplace(photo, std::move(next));
    }
    require(expected != before, "Positive control failed: batch preset is a no-op");
    return expected;
}
void rendered(Context &c, const Document &expected) {
    const auto pixels = fixtureReference(c, expected, c.editor.sourcePath());
    c.driver->wait(
        [&] {
            auto *image = c.driver->item("develop.preview");
            return !c.editor.busy() && image->isVisible() &&
                   image->property("status").toInt() == 1 &&
                   image->property("source").toUrl().toString() == c.editor.previewUrl() &&
                   c.provider->requestImage({}, nullptr, {}) == pixels;
        },
        "Exact full-document preview Ready/current source/pixels");
    require(c.editor.currentSettings() == expected && c.editor.error().isEmpty(),
            "Preview changed the expected full document");
    c.driver->frame();
}
void saved(Context &c, const Document &expected) {
    c.saved();
    const auto asset =
        luma::Catalog(c.catalogFile.toStdString()).assetByID(c.editor.photoID().toStdString());
    require(asset && asset->edits == expected, "Current full document is not durable");
}
Document exposure(Context &c, QPointF fraction) {
    c.reveal("develop.exposureSlider");
    const auto before = c.editor.currentSettings();
    c.driver->drag("develop.exposureSlider", c.driver->sliderThumb("develop.exposureSlider"),
                   fraction);
    const auto after = c.editor.currentSettings();
    auto expected = before;
    expected.exposure = after.exposure;
    require(after != before && after == expected && c.editor.error().isEmpty(),
            "Positive exposure input failed or changed unrelated fields");
    return expected;
}
void repeatedBatchCancel(Context &c) {
    c.import(100);
    selected(c, 100);
    const auto before = documents(c);
    const auto beforeRevisions = revisions(c);
    const auto expected = expectedPreset(c, before);
    const auto selection = c.library.selectionIDs();
    const ReleaseWrites release{c.storageFault};
    c.storageFault.holdWrites(true);
    c.driver->click("batchApplyPreset");
    c.driver->wait(
        [&] {
            return c.storageFault.writeBlocked() && c.batch.working() && c.batch.progress() > 0 &&
                   c.driver->object("batch.wait")->property("opened").toBool();
        },
        "Real batch processed rows and reached a held catalog/WAL write");
    require(documents(c) == before && revisions(c) == beforeRevisions,
            "Uncommitted batch leaked through the durable SQLite reader snapshot");
    int idleTransitions = 0;
    bool wasWorking = true;
    QObject observer;
    QObject::connect(&c.batch, &BatchController::changed, &observer, [&] {
        const bool now = c.batch.working();
        if (wasWorking && !now)
            ++idleTransitions;
        wasWorking = now;
    });
    for (int i = 0; i < 25; ++i) {
        require(c.driver->item("batch.cancel")->isVisible() &&
                    c.driver->item("batch.cancel")->isEnabled(),
                "Repeated batch Cancel lost its real active UI action");
        c.driver->click("batch.cancel");
        require(c.batch.working() && c.storageFault.writeBlocked(),
                "Batch escaped the held WAL boundary before all Cancel inputs");
    }
    c.storageFault.holdWrites(false);
    batchIdle(c);
    const auto after = documents(c);
    require(idleTransitions == 1 && c.batch.error().isEmpty() &&
                (after == before || after == expected) && c.library.selectionIDs() == selection,
            "Repeated Cancel caused duplicate completion or partial durable rows");
    const bool committed = after == expected;
    auto expectedRevisions = beforeRevisions;
    if (committed)
        for (auto &[_, revision] : expectedRevisions)
            ++revision;
    require(revisions(c) == expectedRevisions && c.batch.canUndo() == committed &&
                !c.batch.canRedo(),
            "Late Cancel left history/revisions inconsistent with atomic winner");
    c.driver->log({{"action", "repeated_cancel_wal_winner"},
                   {"winner", committed ? "fully_committed" : "rolled_back"},
                   {"cancel_inputs", 25},
                   {"idle_transitions", idleTransitions}});
    if (committed) {
        c.driver->click("batchUndo");
        batchIdle(c);
        require(documents(c) == before && !c.batch.canUndo() && c.batch.canRedo(),
                "Fully committed late-cancel winner did not retain exactly one safe Undo");
    }
    // Recovery positive control: normal full transaction and its one Undo still work.
    c.driver->click("batchApplyPreset");
    batchIdle(c);
    require(documents(c) == expected && c.batch.canUndo(), "Cancel left batch unable to commit");
    c.driver->click("batchUndo");
    batchIdle(c);
    require(documents(c) == before && c.batch.canRedo(), "Cancel recovery Undo lost baseline");
    c.checkData();
}
void batchArbitration(Context &c) {
    c.openFirst();
    const auto photo = c.editor.photoID().toStdString();
    const auto prior = exposure(c, {.58, .5});
    saved(c, prior);
    selected(c, 10);
    const auto baseline = documents(c);
    const auto batchDocs = expectedPreset(c, baseline);
    c.driver->click("batchApplyPreset");
    batchIdle(c);
    require(documents(c) == batchDocs && c.editor.currentSettings() == batchDocs.at(photo) &&
                !c.editor.canUndo() && !c.editor.canRedo() && c.batch.canUndo(),
            "Committed active-asset batch did not establish its independent history boundary");
    c.driver->click("main.developButton");
    const auto edited = exposure(c, {.43, .5});
    saved(c, edited);
    auto editedDocs = batchDocs;
    editedDocs.at(photo) = edited;
    require(c.editor.canUndo() && c.batch.canUndo(), "Arbitration fixture lacks both Undo owners");
    c.driver->click("main.undoButton"); // Develop: editor Undo has priority.
    require(c.editor.currentSettings() == batchDocs.at(photo) && !c.editor.canUndo() &&
                c.editor.canRedo() && c.batch.canUndo(),
            "Develop Undo did not select editor history");
    saved(c, batchDocs.at(photo));
    require(documents(c) == batchDocs, "Editor Undo changed another selected photo");
    c.driver->click("main.redoButton");
    require(c.editor.currentSettings() == edited && c.batch.canUndo(),
            "Develop Redo selected batch");
    saved(c, edited);
    require(documents(c) == editedDocs, "Editor Redo changed another photo");
    c.driver->click("main.undoButton");
    saved(c, batchDocs.at(photo));
    c.driver->click("main.undoButton"); // No local Undo remains: route to batch.
    batchIdle(c);
    require(documents(c) == baseline && c.editor.currentSettings() == prior &&
                !c.editor.canUndo() && !c.editor.canRedo() && c.batch.canRedo(),
            "Develop fallback Undo did not atomically restore the prior batch across all photos");
    c.driver->click("main.redoButton");
    batchIdle(c);
    require(documents(c) == batchDocs && c.editor.currentSettings() == batchDocs.at(photo) &&
                !c.editor.canRedo(),
            "Develop fallback Redo did not restore batch snapshot");
    const auto newer = exposure(c, {.4, .5});
    saved(c, newer);
    auto conflicting = batchDocs;
    conflicting.at(photo) = newer;
    c.driver->click("main.libraryButton");
    c.driver->click("main.undoButton"); // Library deliberately selects batch despite local Undo.
    batchIdle(c);
    require(!c.batch.error().isEmpty() && documents(c) == conflicting &&
                c.editor.currentSettings() == newer && c.editor.canUndo() && c.batch.canUndo(),
            "Library arbitration silently selected editor or overwrote a newer edit on conflict");
    c.driver->click("main.developButton");
    c.driver->click("main.undoButton");
    saved(c, batchDocs.at(photo));
    rendered(c, batchDocs.at(photo));
    c.driver->click("main.libraryButton");
    c.driver->click("main.undoButton");
    batchIdle(c);
    require(c.batch.error().isEmpty() && documents(c) == baseline && c.batch.canRedo(),
            "Safe Library batch Undo did not recover after local conflict was undone");
    c.driver->click("main.developButton");
    rendered(c, prior);
    saved(c, prior);
    c.checkData();
}
struct Footprint {
    std::int64_t pages = 0, pageSize = 0, freePages = 0;
    std::map<QString, std::int64_t> rows;
};
Footprint footprint(Context &c) {
    Database db(c.catalogFile);
    Footprint result{db.scalar("PRAGMA page_count"),
                     db.scalar("PRAGMA page_size"),
                     db.scalar("PRAGMA freelist_count"),
                     {}};
    sqlite3_stmt *raw = nullptr;
    require(sqlite3_prepare_v2(db.handle,
                               "SELECT name FROM sqlite_master WHERE type='table' AND name NOT "
                               "LIKE 'sqlite_%' ORDER BY name",
                               -1, &raw, nullptr) == SQLITE_OK,
            "Cannot inspect permanent catalog schema");
    const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw,
                                                                               sqlite3_finalize);
    int code = SQLITE_ROW;
    while ((code = sqlite3_step(raw)) == SQLITE_ROW) {
        const auto name =
            QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(raw, 0)));
        auto quoted = name;
        quoted.replace('"', "\"\"");
        result.rows.emplace(name, db.scalar(("SELECT COUNT(*) FROM \"" + quoted + "\"").toUtf8()));
    }
    require(code == SQLITE_DONE, "Permanent catalog schema query did not finish");
    return result;
}
void checkpoint(Context &c) {
    Database db(c.catalogFile);
    c.driver->wait(
        [&] {
            int log = -1, done = -1;
            const int result = sqlite3_wal_checkpoint_v2(db.handle, nullptr,
                                                         SQLITE_CHECKPOINT_TRUNCATE, &log, &done);
            require(result == SQLITE_OK || result == SQLITE_BUSY, "Catalog checkpoint failed");
            return result == SQLITE_OK && log == done;
        },
        "Catalog WAL checkpoint for like-for-like footprint measurement", 10000);
}
void batchGrowth(Context &c) {
    c.import(100);
    selected(c, 100);
    // Fixture preparation only: keep revision integer storage width fixed across all
    // 200 transactions, avoiding legitimate SQLite row growth at varint boundaries.
    {
        Database db(c.catalogFile);
        db.execute("UPDATE edit_documents SET revision=1000000");
    }
    const auto baseline = documents(c);
    const auto baseRevisions = revisions(c);
    const auto expected = expectedPreset(c, baseline);
    const auto selection = c.library.selectionIDs();
    const auto before = footprint(c);
    std::optional<Footprint> warmed;
    for (int cycle = 0; cycle < 100; ++cycle) {
        c.driver->click("batchApplyPreset");
        batchIdle(c);
        require(c.batch.error().isEmpty() && c.batch.canUndo() && documents(c) == expected,
                "Stress apply did not commit every exact expected document");
        c.driver->click("batchUndo");
        batchIdle(c);
        require(c.batch.error().isEmpty() && c.batch.canRedo() && !c.batch.canUndo() &&
                    documents(c) == baseline && c.library.selectionIDs() == selection,
                "Stress Undo lost exact durable baseline/selection/history");
        auto expectedRevisions = baseRevisions;
        for (auto &[_, revision] : expectedRevisions)
            revision += std::int64_t{2} * (cycle + 1);
        require(revisions(c) == expectedRevisions,
                "One apply/Undo cycle produced missing/duplicate durable revisions");
        const auto measured = footprint(c);
        require(measured.rows == before.rows,
                "Repeated batch cycles accumulated permanent catalog rows");
        if (!warmed)
            warmed = measured;
        c.driver->log({{"action", "batch_apply_undo_growth"},
                       {"cycle", cycle + 1},
                       {"main_pages", measured.pages},
                       {"main_free_pages", measured.freePages},
                       {"main_page_bytes", measured.pageSize},
                       {"worker_temp_journal_observed", false}});
    }
    checkpoint(c);
    const auto final = footprint(c);
    require(warmed && final.pageSize == warmed->pageSize && final.pages <= warmed->pages &&
                final.rows == warmed->rows,
            "Fixed-size repeated batch workload grew permanent catalog pages after warmup");
    require(QFileInfo(c.catalogFile).size() == final.pages * final.pageSize,
            "Checkpointed main catalog file does not match its measured page footprint");
    c.checkData();
}
void popup(Context &c) {
    c.driver->click("updates.bell");
    c.driver->wait([&] { return c.driver->object("updates.popup")->property("opened").toBool(); },
                   "Real update notification popup opened");
}
void dismiss(Context &c) {
    c.driver->key(Qt::Key_Escape);
    c.driver->wait([&] { return !c.driver->object("updates.popup")->property("visible").toBool(); },
                   "Update popup closed by real Escape input");
}
void history(Context &c, const Document &before, const Document &after) {
    c.driver->click("main.undoButton");
    require(c.editor.currentSettings() == before && !c.editor.canUndo(),
            "Update UI/gesture polluted the one-entry editor history");
    rendered(c, before);
    c.driver->click("main.redoButton");
    require(c.editor.currentSettings() == after, "Update UI/gesture Redo lost the full state");
    rendered(c, after);
    saved(c, after);
}
void idlePopup(Context &c) {
    c.openFirst();
    saved(c, c.editor.currentSettings());
    const auto before = c.editor.currentSettings();
    const auto photo = c.editor.photoID();
    const auto revision = revisions(c);
    const auto durable = documents(c);
    for (int cycle = 0; cycle < 3; ++cycle) {
        popup(c);
        require(!c.driver->object("updates.popup")->property("modal").toBool(),
                "Idle notification unexpectedly became an application modal");
        const auto samples = c.metrics.json()["rss_samples"].toInteger();
        c.driver->wait([&] { return c.metrics.json()["rss_samples"].toInteger() >= samples + 2; },
                       "UI heartbeat continues while idle update popup is open");
        require(c.editor.currentSettings() == before && c.editor.photoID() == photo &&
                    !c.editor.canUndo() && !c.editor.canRedo() && revisions(c) == revision &&
                    documents(c) == durable,
                "Idle update popup changed photo/edit/history/durable revision");
        dismiss(c);
    }
    c.driver->click("main.libraryButton");
    c.driver->click("main.developButton");
    const auto changed = exposure(c, {.6, .5});
    rendered(c, changed);
    history(c, before, changed);
}
NativeUpdaterProbe &sdk(Context &c) {
    require(NativeUpdaterProbe::active,
            "Case requires Context(..., updaterProbe=true) factory fixture");
    c.driver->wait([&] { return NativeUpdaterProbe::active->starts == 1 && c.updates.canCheck(); },
                   "Actual test SDK adapter start callback completed");
    return *NativeUpdaterProbe::active;
}
void checkFailure(Context &c) {
    auto &probe = sdk(c);
    c.openFirst();
    const auto before = c.editor.currentSettings();
    saved(c, before);
    const auto originalRevisions = revisions(c);
    const auto originalDocuments = documents(c);
    const auto originalPhoto = c.editor.photoID();
    const auto originalSource = c.editor.sourcePath();
    bool observedChecking = false;
    QObject observer;
    QObject::connect(&c.updates, &UpdateController::changed, &observer,
                     [&] { observedChecking = observedChecking || c.updates.checking(); });
    popup(c);
    require(c.driver->item("updates.check")->isEnabled(),
            "Real update Check action is unavailable");
    c.driver->log({{"action", "sdk_probe_check_failure_fixture"},
                   {"sdk_interface_callback_fixture", true},
                   {"real_sparkle_network_exercised", false},
                   {"controller_status_setter_used", false}});
    c.driver->click("updates.check");
    c.driver->wait([&] { return probe.failuresDelivered == 1; },
                   "NativeUpdater interface callback delivered controlled check failure");
    require(probe.protocolFailure.isEmpty() && probe.checks == 1 && observedChecking &&
                !c.updates.checking() && !c.updates.restartPending() && c.updates.canCheck() &&
                c.driver->item("updates.message")->isVisible() &&
                c.driver->object("updates.message")->property("text").toString() ==
                    "Could not update Luma: UX controlled SDK check failure" &&
                c.editor.currentSettings() == before && revisions(c) == originalRevisions &&
                documents(c) == originalDocuments && c.editor.photoID() == originalPhoto &&
                c.editor.sourcePath() == originalSource,
            "Check failure stayed pending, hid its message or mutated editor/catalog state");
    dismiss(c);
    const auto changed = exposure(c, {.61, .5});
    rendered(c, changed);
    history(c, before, changed);
    writeJson(c.root + "/measurements.json", {{"mode", "SDK_INTERFACE_CALLBACK_FIXTURE"},
                                              {"sdk_interface_callback_fixture", true},
                                              {"real_sparkle_network_exercised", false},
                                              {"controller_status_setter_used", false},
                                              {"adapter_starts", probe.starts},
                                              {"checks", probe.checks},
                                              {"failures_delivered", probe.failuresDelivered}});
}
void notifyDuringSlider(Context &c) {
    const KeyboardNavigation navigation;
    auto &probe = sdk(c);
    c.openFirst();
    const auto before = c.editor.currentSettings();
    c.reveal("develop.exposureSlider");
    auto *slider = c.driver->item("develop.exposureSlider");
    for (int step = 0; step < 240 && !slider->hasActiveFocus(); ++step)
        c.driver->key(Qt::Key_Tab);
    c.reveal("develop.exposureSlider");
    require(slider->hasActiveFocus() && c.editor.currentSettings() == before,
            "Actual Tab input could not focus a pristine visible Slider");
    ReleasePointer pointer{c, c.driver->point("develop.exposureSlider", {.6, .5})};
    c.driver->press("develop.exposureSlider", c.driver->sliderThumb("develop.exposureSlider"));
    pointer.down = true;
    c.driver->move("develop.exposureSlider", {.6, .5});
    const auto partial = c.editor.currentSettings();
    auto expectedPartial = before;
    expectedPartial.exposure = partial.exposure;
    require(partial != before && partial == expectedPartial && slider->property("pressed").toBool(),
            "Notification fixture never held a changed real Slider gesture");
    const QPointer<QQuickItem> focus = c.window->activeFocusItem();
    const QPointer<QWindow> focusWindow = QGuiApplication::focusWindow();
    require(focus && focus.data() == slider && focusWindow.data() == c.window,
            "Held Slider lacks real active focus/window before SDK notification");
    c.driver->log({{"action", "sdk_found_callback_during_real_held_slider"},
                   {"sdk_interface_callback_fixture", true},
                   {"real_sparkle_network_exercised", false},
                   {"popup_open_requested", false}});
    probe.foundAsync("0.5.99-fixture");
    c.driver->wait([&] { return probe.foundDelivered == 1; },
                   "SDK found callback delivered on GUI loop");
    c.driver->frame();
    require(probe.protocolFailure.isEmpty() && c.updates.availableVersion() == "0.5.99-fixture" &&
                c.driver->item("updates.availableBadge")->isVisible() &&
                !c.driver->object("updates.popup")->property("visible").toBool() && focus &&
                c.window->activeFocusItem() == focus.data() &&
                QGuiApplication::focusWindow() == focusWindow.data() &&
                slider->property("pressed").toBool() && c.editor.currentSettings() == partial,
            "Automatic update notification stole focus/pointer or mutated the active Slider");
    pointer.last = c.driver->point("develop.exposureSlider", {.7, .5});
    c.driver->move("develop.exposureSlider", {.7, .5});
    c.driver->release("develop.exposureSlider", {.7, .5});
    pointer.down = false;
    const auto final = c.editor.currentSettings();
    auto expected = before;
    expected.exposure = final.exposure;
    require(final == expected && final.exposure != partial.exposure &&
                !slider->property("pressed").toBool(),
            "Slider failed to continue and complete after SDK badge notification");
    rendered(c, final);
    history(c, before, final);
    writeJson(c.root + "/measurements.json", {{"mode", "SDK_INTERFACE_CALLBACK_FIXTURE"},
                                              {"sdk_interface_callback_fixture", true},
                                              {"real_sparkle_network_exercised", false},
                                              {"controller_status_setter_used", false},
                                              {"adapter_starts", probe.starts},
                                              {"notifications_delivered", probe.foundDelivered}});
}
QByteArray digest(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read owned close/export evidence");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    require(hash.addData(&file), "Cannot hash complete owned export output");
    return hash.result();
}
void closeSaveExport(Context &c, bool overlapCloseReady) {
    c.openFirst();
    const auto photo = c.editor.photoID().toStdString();
    const auto initial = c.exportPNG("close-overlap"); // Real positive export/settings fixture.
    const auto initialHash = digest(initial);
    saved(c, c.editor.currentSettings());
    const ReleaseWrites release{c.storageFault};
    c.storageFault.holdWrites(true);
    const auto final = exposure(c, {.62, .5});
    c.driver->wait([&] { return c.storageFault.writeBlocked() && c.editor.saving(); },
                   "Real changed edit is held at actual catalog/WAL write");
    c.driver->click("exportButton");
    c.driver->wait([&] { return c.driver->object("exportPanel")->property("opened").toBool(); },
                   "Previously validated PNG settings opened during held save");
    c.driver->click("export.current");
    c.driver->wait(
        [&] {
            return c.exporter.working() && c.exporter.waitingForSave() && c.editor.saving() &&
                   c.editor.editingLocked();
        },
        "Real export controller and save overlap before quit");
    require(c.storageFault.writeBlocked() && c.exporter.total() == 0 &&
                c.editor.currentSettings() == final && c.window->isVisible(),
            "Close fixture missed simultaneous save and export wait boundary");
    QSignalSpy editorReady(&c.editor, &EditorController::closeReady);
    QSignalSpy exportReady(&c.exporter, &ExportController::closeReady);
    require(editorReady.isValid() && exportReady.isValid(),
            "Controller closeReady signals unavailable");
    int hiddenTransitions = 0;
    QObject observer;
    QObject::connect(c.window, &QWindow::visibleChanged, &observer, [&](bool visible) {
        if (!visible)
            ++hiddenTransitions;
    });
    std::unique_ptr<QueuedCallGate> gate;
    if (overlapCloseReady) {
        // Waiting Export has no worker completion pending. requestClose synchronously
        // finish(true)'s it and queues exactly one real closeReady MetaCall.
        gate = std::make_unique<QueuedCallGate>(
            c.exporter,
            [&] {
                return !c.exporter.working() && c.editor.saving() && c.storageFault.writeBlocked();
            },
            [&] {
                require(exportReady.count() == 0 && editorReady.count() == 0 &&
                            c.window->isVisible(),
                        "Close-ready race gate did not hold the real queued export callback");
                c.storageFault.holdWrites(false);
                c.driver->wait(
                    [&] { return editorReady.count() >= 1 && !c.editor.saving(); },
                    "Editor emits its own closeReady before queued Export closeReady resumes");
                require(!c.window->isVisible(),
                        "Editor closeReady did not complete guarded window close");
            });
    }
    c.driver->quitShortcut();
    if (gate && gate->failure)
        std::rethrow_exception(gate->failure);
    if (!overlapCloseReady) {
        require(c.window->isVisible() && c.editor.saving() && !c.exporter.working(),
                "Quit did not cancel export while deferring close for pending save");
        c.storageFault.holdWrites(false);
    }
    c.driver->wait(
        [&] {
            return !c.window->isVisible() && !c.editor.saving() && !c.editor.busy() &&
                   !c.editor.exporting() && !c.exporter.working() && !c.batch.working() &&
                   !c.library.importing() && !c.editor.editingLocked();
        },
        "Save/export coordinated shutdown completed without deadlock", 30000);
    if (gate) {
        require(gate->caught, "No actual export closeReady MetaCall reached the race gate");
        if (gate->failure)
            std::rethrow_exception(gate->failure);
    }
    require(hiddenTransitions == 1 && editorReady.count() >= 1 && exportReady.count() == 1,
            "Multiple real controller closeReady notifications produced duplicate/missing closure");
    const auto asset = luma::Catalog(c.catalogFile.toStdString()).assetByID(photo);
    require(asset && asset->edits == final && c.editor.currentSettings() == final &&
                c.editor.error().isEmpty() && c.exporter.failures().isEmpty() &&
                digest(initial) == initialHash &&
                QDir(c.root + "/exports")
                        .entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot)
                        .size() == 1,
            "Close lost the final saved edit, changed existing output or leaked a partial export");
    c.checkData();
}
} // namespace

bool batchUpdateRemainingScenario(Context &c, const QString &id) {
    if (id == "UX-BAT-004")
        repeatedBatchCancel(c);
    else if (id == "UX-BAT-007")
        batchArbitration(c);
    else if (id == "UX-BAT-014")
        batchGrowth(c);
    else if (id == "UX-UPD-001")
        idlePopup(c);
    else if (id == "UX-UPD-005")
        checkFailure(c);
    else if (id == "UX-UPD-006")
        notifyDuringSlider(c);
    else if (id == "UX-APP-011")
        closeSaveExport(c, false);
    else if (id == "UX-RACE-010")
        closeSaveExport(c, true);
    else
        return false;
    return true;
}
} // namespace ux
