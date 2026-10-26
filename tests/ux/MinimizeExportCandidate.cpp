#include "tests/ux/NativeWindowLifecycle.h"
#include "tests/ux/QueuedCallGate.h"
#include "tests/ux/UxContext.h"
#include <QColorSpace>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSGRendererInterface>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <sqlite3.h>
#include <utility>

namespace ux {
namespace {
constexpr auto candidateID = "CANDIDATE-APP016-MINIMIZED-EXPORT";
QByteArray bytes(const QString &path) {
    QFile file(path);
    require(QFileInfo(path).isFile() && !QFileInfo(path).isSymLink() &&
                file.open(QIODevice::ReadOnly),
            "Cannot read owned minimized-export file: " + path);
    return file.readAll();
}
QString digest(const QString &path) {
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes(path), QCryptographicHash::Sha256).toHex());
}
QJsonArray rows(sqlite3 *database, const QByteArray &query) {
    sqlite3_stmt *raw = nullptr;
    const int prepared = sqlite3_prepare_v2(database, query.constData(), -1, &raw, nullptr);
    const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw,
                                                                               sqlite3_finalize);
    require(prepared == SQLITE_OK, "Cannot prepare minimized-export SQL observation");
    QStringList encoded;
    int result = SQLITE_ROW;
    while ((result = sqlite3_step(statement.get())) == SQLITE_ROW) {
        QJsonArray row;
        for (int i = 0; i < sqlite3_column_count(statement.get()); ++i) {
            const int type = sqlite3_column_type(statement.get(), i);
            QJsonValue value;
            if (type == SQLITE_INTEGER)
                value = QString::number(sqlite3_column_int64(statement.get(), i));
            else if (type == SQLITE_FLOAT)
                value = QString::number(sqlite3_column_double(statement.get(), i), 'g', 17);
            else if (type == SQLITE_TEXT)
                value = QString::fromUtf8(
                    reinterpret_cast<const char *>(sqlite3_column_text(statement.get(), i)),
                    sqlite3_column_bytes(statement.get(), i));
            else if (type == SQLITE_BLOB)
                value = QString::fromLatin1(
                    QByteArray(static_cast<const char *>(sqlite3_column_blob(statement.get(), i)),
                               sqlite3_column_bytes(statement.get(), i))
                        .toHex());
            row.append(QJsonObject{{"type", type}, {"value", value}});
        }
        encoded.append(QString::fromUtf8(QJsonDocument(row).toJson(QJsonDocument::Compact)));
    }
    require(result == SQLITE_DONE, "Incomplete minimized-export SQL observation");
    std::sort(encoded.begin(), encoded.end());
    return QJsonArray::fromStringList(encoded);
}
QJsonObject catalog(Context &c) {
    sqlite3 *raw = nullptr;
    const int opened =
        sqlite3_open_v2(c.catalogFile.toUtf8().constData(), &raw, SQLITE_OPEN_READONLY, nullptr);
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
    require(opened == SQLITE_OK &&
                sqlite3_exec(db.get(), "BEGIN", nullptr, nullptr, nullptr) == SQLITE_OK,
            "Cannot open a read-only minimized-export catalog snapshot");
    QJsonObject result;
    result["schema"] = rows(db.get(), "SELECT type,name,tbl_name,sql FROM sqlite_master");
    sqlite3_stmt *statement = nullptr;
    require(sqlite3_prepare_v2(db.get(), "SELECT name FROM sqlite_master WHERE type='table'", -1,
                               &statement, nullptr) == SQLITE_OK,
            "Cannot enumerate minimized-export tables");
    const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> tables(statement,
                                                                            sqlite3_finalize);
    int step = SQLITE_ROW;
    while ((step = sqlite3_step(tables.get())) == SQLITE_ROW) {
        const auto name =
            QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(tables.get(), 0)));
        auto escaped = name;
        escaped.replace('"', "\"\"");
        result[name] = rows(db.get(), "SELECT * FROM \"" + escaped.toUtf8() + '"');
    }
    require(step == SQLITE_DONE, "Incomplete minimized-export table census");
    return result;
}
void pixels(const QString &path, int variant) {
    const std::array<QColor, 2> expected{QColor(40, 80, 120), QColor(160, 120, 80)};
    const QImage image(path);
    require(image.size() == QSize(64, 48) && image.colorSpace().isValid(),
            "Minimized export lost PNG dimensions or its color profile");
    const auto color = expected.at(static_cast<std::size_t>(variant));
    for (int y = 0; y < 48; ++y)
        for (int x = 0; x < 64; ++x) {
            const auto actual = image.pixelColor(x, y);
            require(std::abs(actual.red() - color.red()) <= 1 &&
                        std::abs(actual.green() - color.green()) <= 1 &&
                        std::abs(actual.blue() - color.blue()) <= 1 && actual.alpha() == 255,
                    "Minimized export differs from independent literal source pixels");
        }
}
void reveal(Context &c, const QString &id) {
    auto &d = *c.driver;
    auto *target = d.item(id);
    auto *view = qobject_cast<QQuickItem *>(
        qvariant_cast<QObject *>(d.object("export.scroll")->property("contentItem")));
    require(view && target->isVisible(), "Missing visible Export field: " + id);
    for (int i = 0; i < 40; ++i) {
        d.wait([&] { return !view->property("moving").toBool(); }, "export wheel settled");
        d.frame();
        const auto rect = target->mapRectToItem(view, QRectF(QPointF{}, target->size()));
        if (rect.top() >= 0 && rect.bottom() <= view->height())
            return;
        d.wheel("export.scroll", rect.top() < 0 ? 120 : -120);
    }
    require(false, "Cannot reveal Export field by actual wheel input: " + id);
}
} // namespace
bool minimizeExportCandidate(Context &c, const QString &id) {
    if (id != candidateID)
        return false;
    require(QGuiApplication::platformName() == "cocoa" &&
                c.window->rendererInterface()->graphicsApi() == QSGRendererInterface::Metal,
            "APP016 candidate requires actual Cocoa/Metal; no simulated minimize fallback");
    const auto transport = qEnvironmentVariable("LUMA_UX_WINDOW_LIFECYCLE_MODE");
    require(transport.isEmpty() || transport == "external-native-minimize" ||
                transport == "direct-native-minimize",
            "Unknown native window lifecycle transport");
    const bool externalMinimize = transport == "external-native-minimize";
    auto &d = *c.driver;
    NativeWindowLifecycle native(*c.window);
    QJsonArray checkpoints, deliveryStates;
    QJsonObject evidence{{"case_id", id},
                         {"official_APP016_qualified", false},
                         {"fixture_chooser_result_only", true},
                         {"native_action", "NSWindow performMiniaturize; deminiaturize"},
                         {"physical_pointer_or_dock_input", false},
                         {"minimize_transport", transport.isEmpty() ? "AppKit action" : transport},
                         {"background_operation", "real two-item PNG export queue"},
                         {"load_testing", false}};
    const auto save = [&] {
        evidence["checkpoints"] = checkpoints;
        evidence["export_deliveries"] = deliveryStates;
        QFile file(c.root + "/app016-minimize-evidence.json");
        require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
                "Cannot write minimized-export evidence");
        const auto data = QJsonDocument(evidence).toJson(QJsonDocument::Indented);
        require(file.write(data) == data.size(), "Incomplete minimized-export evidence");
    };
    const auto checkpoint = [&](const QString &stage) {
        const auto state = native.state();
        checkpoints.append(QJsonObject{{"stage", stage},
                                       {"native", state},
                                       {"working", c.exporter.working()},
                                       {"completed", c.exporter.completed()},
                                       {"succeeded", c.exporter.succeeded()}});
        save();
        return state;
    };
    try {
        require(QDir(c.fixture).entryList({"*.png"}, QDir::Files, QDir::Name) ==
                    QStringList{"Photo-0000.png", "Photo-0001.png"},
                "Minimized export requires exactly two pinned literal sources");
        const QStringList sources{c.fixture + "/Photo-0000.png", c.fixture + "/Photo-0001.png"};
        const QStringList hashes{digest(sources[0]), digest(sources[1])};
        c.import(2);
        d.doubleClick(c.card(0));
        c.loaded();
        require(c.editor.currentSettings() == luma::EditDocument{}, "Expected neutral photo");
        c.reveal("develop.exposureSlider");
        d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {.58, .5});
        c.saved();
        const auto edited = c.editor.currentSettings();
        auto expectedEdit = luma::EditDocument{};
        expectedEdit.exposure = edited.exposure;
        require(edited == expectedEdit && edited.exposure > .1 && c.editor.canUndo(),
                "History positive control did not create an exposure-only edit");
        d.click("main.undoButton");
        c.saved();
        require(c.editor.currentSettings() == luma::EditDocument{} && c.editor.canRedo(),
                "History baseline must retain a real Redo after Undo");
        d.click("main.libraryButton");
        d.click("library.selectAll");
        d.wait([&] { return !c.library.selecting() && c.library.selectedCount() == 2; },
               "two owned export sources selected");
        const auto selected = c.library.selectionIDs();
        const auto source = c.editor.sourcePath();
        const bool undo = c.editor.canUndo(), redo = c.editor.canRedo();
        const auto beforeCatalog = catalog(c);
        evidence["catalog_before"] = beforeCatalog;
        evidence["source_sha256"] = QJsonArray::fromStringList(hashes);
        QJsonArray selectedJSON;
        for (const auto &assetID : selected)
            selectedJSON.append(QString::fromStdString(assetID));
        evidence["selected_ids"] = selectedJSON;
        const auto directory = c.root + "/exports/minimized";
        require(QDir().mkpath(directory), "Cannot create own minimized-export destination");
        const QStringList outputs{directory + "/Photo-0000.png", directory + "/Photo-0001.png"};
        d.click("exportButton");
        d.wait([&] { return d.object("exportPanel")->property("opened").toBool(); },
               "minimized-export form opened");
        d.type("export.destination", directory);
        d.type("exportNaming", "{filename}");
        const std::array<std::pair<const char *, int>, 7> choices{{{"exportFormat", 1},
                                                                   {"export.conflict", 0},
                                                                   {"export.colorSpace", 0},
                                                                   {"export.resize", 0},
                                                                   {"export.outputMedium", 0},
                                                                   {"export.metadata", 0},
                                                                   {"export.watermarkKind", 0}}};
        for (const auto &[selector, index] : choices) {
            reveal(c, selector);
            d.choose(selector, index);
        }
        const auto form = d.object("exportPanel")->property("form").toMap();
        require(form["destination"].toString() == directory &&
                    form["naming"].toString() == "{filename}" && form["format"].toInt() == 1 &&
                    form["resize"].toInt() == 0 && form["watermarkKind"].toInt() == 0,
                "Minimized export form differs from its literal settings");
        const auto baseline = checkpoint("baseline");
        require(!baseline["miniaturized"].toBool() && baseline["key"].toBool() &&
                    baseline["did_miniaturize"].toInt() == 0 &&
                    baseline["did_deminiaturize"].toInt() == 0,
                "Native lifecycle observer lacks an active unminimized baseline");
        bool releasedWhileMinimized = false, remainedMinimized = true;
        std::exception_ptr deliveryFailure;
        QObject deliveryObserver;
        QObject::connect(&c.exporter, &ExportController::changed, &deliveryObserver, [&] {
            if (releasedWhileMinimized && c.exporter.completed() > 0) {
                try {
                    const auto state = native.state();
                    remainedMinimized = remainedMinimized && state["miniaturized"].toBool() &&
                                        state["did_deminiaturize"].toInt() == 0;
                    deliveryStates.append(QJsonObject{{"completed", c.exporter.completed()},
                                                      {"working", c.exporter.working()},
                                                      {"native", state}});
                } catch (...) {
                    deliveryFailure = std::current_exception();
                }
            }
        });
        QString settingsHash, firstOutputHash;
        QueuedCallGate gate(
            c.exporter,
            [&] {
                return c.exporter.working() && !c.exporter.waitingForSave() &&
                       !c.exporter.paused() && c.exporter.total() == 2 &&
                       c.exporter.completed() == 0 && c.exporter.progress() == 1 &&
                       c.exporter.currentFile() == "Photo-0000.png";
            },
            [&] {
                require(QFileInfo::exists(outputs[0]) && !QFileInfo::exists(outputs[1]) &&
                            c.exporter.settings() == form,
                        "Gate must hold the genuine first completion before the second starts");
                pixels(outputs[0], 0);
                firstOutputHash = digest(outputs[0]);
                settingsHash = digest(c.stateRoot + "/export-settings.json");
                checkpoint("first_output_committed_before_minimize");
                if (externalMinimize) {
                    c.window->setTitle(QStringLiteral("Luma QA — экспорт и сворачивание"));
                    const auto markerPath = c.root + "/app016-external-minimize-ready.json";
                    require(!QFileInfo::exists(markerPath), "Minimize marker must be fresh");
                    const auto marker = QJsonObject{
                        {"schema", 1},
                        {"case_id", id},
                        {"nonce", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                        {"created_at_utc",
                         QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                        {"own_pid", QCoreApplication::applicationPid()},
                        {"canonical_executable",
                         QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath()},
                        {"native", native.state()},
                        {"window_title", c.window->title()},
                        {"observation_deadline_ms", 45000},
                        {"dispatch_is_not_transition_proof", true}};
                    QSaveFile markerFile(markerPath);
                    markerFile.setDirectWriteFallback(false);
                    const auto data = QJsonDocument(marker).toJson(QJsonDocument::Indented);
                    require(markerFile.open(QIODevice::WriteOnly) &&
                                markerFile.write(data) == data.size() && markerFile.commit(),
                            "Cannot publish owned external minimize marker");
                    d.log({{"action", "external_native_minimize_ready"}, {"marker", markerPath}});
                } else {
                    d.log({{"action", "native_perform_miniaturize"}});
                    native.minimize(transport == "direct-native-minimize");
                }
                d.wait(
                    [&] {
                        const auto state = native.state();
                        return state["miniaturized"].toBool() &&
                               state["did_miniaturize"].toInt() == 1 &&
                               c.window->windowState() == Qt::WindowMinimized;
                    },
                    "actual NSWindow and Qt miniaturized notification",
                    externalMinimize ? 45000 : 10000);
                checkpoint("minimized_before_real_completion_release");
                require(c.exporter.completed() == 0 && !QFileInfo::exists(outputs[1]) &&
                            c.exporter.working(),
                        "Nested minimize event loop escaped the actual first-completion gate");
                releasedWhileMinimized = true;
                // Return false from the filter: dispatch the original queued callback.
            });
        d.click("export.selection");
        d.wait([&] { return gate.caught; }, "first real export completion", 30000);
        if (gate.failure)
            std::rethrow_exception(gate.failure);
        d.wait([&] { return !c.exporter.working(); }, "real queue finishes while minimized", 30000);
        if (deliveryFailure)
            std::rethrow_exception(deliveryFailure);
        const auto finished = checkpoint("second_output_finished_while_minimized");
        QObject::disconnect(&c.exporter, nullptr, &deliveryObserver, nullptr);
        require(releasedWhileMinimized && remainedMinimized && !deliveryStates.isEmpty() &&
                    finished["miniaturized"].toBool() &&
                    finished["did_deminiaturize"].toInt() == 0 &&
                    c.window->windowState() == Qt::WindowMinimized && c.exporter.total() == 2 &&
                    c.exporter.completed() == 2 && c.exporter.succeeded() == 2 &&
                    c.exporter.skipped() == 0 && c.exporter.error().isEmpty() &&
                    c.exporter.failures().isEmpty(),
                "Background queue did not finish both real items while actually minimized");
        for (int i = 0; i < 2; ++i)
            pixels(outputs[i], i);
        require(digest(outputs[0]) == firstOutputHash,
                "Background completion rewrote the already committed first output");
        require(catalog(c) == beforeCatalog, "Minimized export changed the logical catalog");
        d.log({{"action", "native_deminiaturize"}});
        native.restore();
        d.wait(
            [&] {
                const auto state = native.state();
                return !state["miniaturized"].toBool() && state["did_deminiaturize"].toInt() == 1 &&
                       state["visible"].toBool() && state["key"].toBool() && c.window->isActive() &&
                       c.window->windowState() == Qt::WindowNoState;
            },
            "same native window restored naturally", 10000);
        const auto restored = checkpoint("restored");
        d.frame();
        d.screenshot(c.root + "/app016-restored.png");
        require(restored["window_number"] == baseline["window_number"] &&
                    restored["did_miniaturize"].toInt() == 1 && c.editor.sourcePath() == source &&
                    c.editor.currentSettings() == luma::EditDocument{} &&
                    c.editor.canUndo() == undo && c.editor.canRedo() == redo &&
                    c.library.selectionIDs() == selected && c.exporter.settings() == form &&
                    digest(c.stateRoot + "/export-settings.json") == settingsHash &&
                    catalog(c) == beforeCatalog,
                "Restore changed window identity, documents, selection, settings or history");
        require(QDir(directory).entryList(QDir::Files | QDir::Hidden, QDir::Name) ==
                    QStringList{"Photo-0000.png", "Photo-0001.png"},
                "Background export created a duplicate or temporary output");
        for (int i = 0; i < 2; ++i)
            require(digest(sources[i]) == hashes[i], "Original photo changed during lifecycle");
        evidence["catalog_after"] = catalog(c);
        evidence["output_sha256"] = QJsonArray{digest(outputs[0]), digest(outputs[1])};
        // Prove the real Redo is still usable after restore, then return to the baseline.
        d.click("main.developButton");
        d.click("main.redoButton");
        c.saved();
        require(c.editor.currentSettings() == edited, "Restored history Redo lost the actual edit");
        d.click("main.undoButton");
        c.saved();
        require(c.editor.currentSettings() == luma::EditDocument{} && c.editor.canRedo(),
                "Restored history Undo failed to recover the neutral document");
        c.checkData();
        evidence["status"] = "PASS";
        evidence["history_redo_undo_positive_control"] = true;
        save();
    } catch (...) {
        try {
            checkpoint("failure_observed_native_state");
        } catch (...) {
            evidence["failure_native_observation_unavailable"] = true;
        }
        evidence["status"] = "FAIL";
        save();
        throw;
    }
    return true;
}
} // namespace ux
