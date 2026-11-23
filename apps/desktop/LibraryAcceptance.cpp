#include "apps/desktop/LibraryAcceptance.h"
#include "apps/desktop/EditorController.h"
#include "apps/desktop/LibraryModel.h"
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQuickItem>
#include <QTimer>
#include <iostream>

void startLibraryAcceptance(QGuiApplication &app, QQuickWindow *window, EditorController &editor,
                            LibraryModel &library, const QString &source, const QString &screenshot,
                            bool closeImport) {
    struct State {
        int phase = 0;
        QElapsedTimer clock;
        qint64 captureAfter = 0;
        QString thumbnailBeforeEdit;
        QString originalID;
        int reviewPhase = 0;
        int sortPhase = 0;
        QStringList reviewIDs;
        QObject *survey = nullptr;
        QStringList surveyIDs;
        bool surveyChecked = false;
    };
    auto state = std::make_shared<State>();
    state->clock.start();
    auto *timer = new QTimer(&app);
    timer->setInterval(25);
    QObject::connect(
        timer, &QTimer::timeout, &app,
        [&app, window, &editor, &library, source, screenshot, closeImport, state] {
            if (state->clock.elapsed() > 90000 || !library.error().isEmpty() ||
                !editor.error().isEmpty()) {
                std::cerr << "Library acceptance failed: " << library.error().toStdString() << " "
                          << editor.error().toStdString() << '\n';
                app.exit(1);
                return;
            }
            if (library.working() || library.loading())
                return;
            switch (state->phase) {
            case 0:
                state->phase = 1;
                library.importFiles({QUrl::fromLocalFile(source)}, false, {}, true, true);
                if (closeImport) {
                    std::cout << "Library close during import started\n";
                    window->close();
                }
                break;
            case 1:
                if (library.count() != 2) {
                    std::cerr << "Expected two public Library fixtures\n";
                    app.exit(1);
                    return;
                }
                state->phase = 2;
                library.selectAll();
                break;
            case 2:
                if (state->surveyChecked && state->sortPhase < 4) {
                    const auto first = library.reviewPhotoAt(0);
                    if (first.isEmpty() || library.reviewPhotoAt(1).isEmpty())
                        return;
                    if (state->sortPhase == 0) {
                        library.select(1);
                        library.rate(5);
                    } else if (state->sortPhase == 1) {
                        library.setSortOrder(static_cast<int>(luma::LibraryOrder::highestRating));
                    } else if (state->sortPhase == 2) {
                        if (first != state->reviewIDs.last() || library.selectedCount() != 1) {
                            std::cerr << "Sorting did not keep the rated photo and selection\n";
                            app.exit(1);
                            return;
                        }
                        library.setSortOrder(static_cast<int>(luma::LibraryOrder::filename));
                    } else {
                        library.selectAll();
                    }
                    ++state->sortPhase;
                    return;
                }
                if (library.selectedCount() != 2)
                    return;
                {
                    auto *pane = window->findChild<QObject *>(QStringLiteral("library.pane"));
                    const auto requestView = [pane](const QString &view) {
                        QVariant accepted;
                        return pane &&
                               QMetaObject::invokeMethod(pane, "requestView",
                                                         Q_RETURN_ARG(QVariant, accepted),
                                                         Q_ARG(QVariant, QVariant(view))) &&
                               accepted.toBool();
                    };
                    const auto require = [&](bool valid, const char *message) {
                        if (!valid) {
                            std::cerr << message << '\n';
                            app.exit(1);
                        }
                        return valid;
                    };
                    const auto comparison = editor.comparison();
                    if (comparison.value("busy").toBool())
                        return;
                    const auto mode = pane ? pane->property("viewMode").toString() : QString{};
                    const auto rendered = comparison.value("ids").toStringList();
                    switch (state->reviewPhase) {
                    case 0: {
                        const auto first = library.reviewPhotoAt(0),
                                   second = library.reviewPhotoAt(1);
                        if (!first.isEmpty() && !second.isEmpty() &&
                            require(requestView(QStringLiteral("loupe")),
                                    "Cannot open Library Loupe")) {
                            state->reviewIDs = {first, second};
                            ++state->reviewPhase;
                        }
                        return;
                    }
                    case 1:
                        if (require(mode == "loupe" && comparison.value("ready").toBool() &&
                                        rendered == QStringList{state->reviewIDs.first()} &&
                                        !editor.hasPhoto(),
                                    "Loupe opened the wrong photo or entered Develop") &&
                            require(QMetaObject::invokeMethod(pane, "moveLoupe",
                                                              Q_ARG(QVariant, QVariant(1))),
                                    "Cannot navigate to the next photo in Loupe"))
                            ++state->reviewPhase;
                        return;
                    case 2:
                        if (require(mode == "loupe" && comparison.value("ready").toBool() &&
                                        rendered == QStringList{state->reviewIDs.last()} &&
                                        library.selectedCount() == 2,
                                    "Loupe navigation changed the multi-photo selection") &&
                            require(requestView(QStringLiteral("compare")),
                                    "Cannot open Library Compare"))
                            ++state->reviewPhase;
                        return;
                    case 3:
                        if (require(mode == "compare" && comparison.value("ready").toBool() &&
                                        rendered == QStringList{state->reviewIDs.last(),
                                                                state->reviewIDs.first()} &&
                                        !editor.hasPhoto(),
                                    "Compare did not keep the active selection first") &&
                            require(requestView(QStringLiteral("grid")),
                                    "Cannot return from Compare to Grid"))
                            ++state->reviewPhase;
                        return;
                    case 4:
                        if (mode != "grid" || !rendered.isEmpty())
                            return;
                        ++state->reviewPhase;
                        break;
                    default:
                        break;
                    }
                }
                if (!state->surveyChecked) {
                    if (!state->survey) {
                        state->survey =
                            window->findChild<QObject *>(QStringLiteral("library.survey"));
                        state->surveyIDs = library.selectedAssetIDs();
                        if (!state->survey ||
                            !QMetaObject::invokeMethod(state->survey, "showSelection")) {
                            std::cerr << "Cannot open the Survey dialog\n";
                            app.exit(1);
                        }
                        return;
                    }
                    const auto comparison = editor.comparison();
                    if (comparison.value("busy").toBool())
                        return;
                    if (!comparison.value("ready").toBool()) {
                        std::cerr << "Survey preview failed: "
                                  << comparison.value("error").toString().toStdString() << '\n';
                        app.exit(1);
                        return;
                    }
                    const auto findItem = [](QQuickItem *root, const QString &name,
                                             auto &&find) -> QQuickItem * {
                        if (root->objectName() == name)
                            return root;
                        for (auto *child : root->childItems())
                            if (auto *item = find(child, name, find))
                                return item;
                        return nullptr;
                    };
                    for (const auto &id : state->surveyIDs) {
                        const auto *image = findItem(window->contentItem(),
                                                     "library.surveyPreview." + id, findItem);
                        if (!image || image->property("status").toInt() != 1)
                            return;
                    }
                    auto *remove =
                        findItem(window->contentItem(),
                                 "library.surveyRemove." + state->surveyIDs.first(), findItem);
                    if (!remove || !QMetaObject::invokeMethod(remove, "clicked") ||
                        library.selectedAssetIDs() != QStringList{state->surveyIDs.last()} ||
                        editor.hasPhoto() || !QMetaObject::invokeMethod(state->survey, "close")) {
                        std::cerr << "Survey changed the wrong selection or active photo\n";
                        app.exit(1);
                        return;
                    }
                    state->surveyChecked = true;
                    library.selectAll();
                    return;
                }
                state->phase = 3;
                library.rate(4);
                break;
            case 3:
                state->phase = 4;
                library.createCollection(QStringLiteral("GUI selection"));
                break;
            case 4:
                if (library.collections().isEmpty())
                    return;
                state->phase = 5;
                library.collectionMembership(
                    library.collections().first().toMap().value(QStringLiteral("id")).toString(),
                    true);
                break;
            case 5:
                state->phase = 6;
                library.flag(1);
                break;
            case 6:
                state->phase = 7;
                library.label(QStringLiteral("blue"));
                state->captureAfter = state->clock.elapsed() + 1800;
                break;
            case 7:
                if (state->clock.elapsed() < state->captureAfter)
                    return;
                if (!screenshot.isEmpty() && !window->grabWindow().save(screenshot)) {
                    std::cerr << "Cannot capture Library\n";
                    app.exit(1);
                    return;
                }
                if (library.data(library.index(0), LibraryModel::AssetID).toString().isEmpty())
                    return;
                state->phase = 8;
                library.select(0);
                library.activate(0);
                break;
            case 8:
                if (editor.busy() || !editor.hasPhoto())
                    return;
                state->phase = 9;
                state->thumbnailBeforeEdit =
                    library.data(library.index(0), LibraryModel::Thumbnail).toString();
                editor.setExposure(1.75);
                break;
            case 9:
                if (editor.busy() || editor.saving() || editor.previewUrl().isEmpty())
                    return;
                {
                    const auto current =
                        library.data(library.index(0), LibraryModel::Thumbnail).toString();
                    if (current == state->thumbnailBeforeEdit)
                        return;
                    const std::function<bool(QQuickItem *)> ready = [&](QQuickItem *item) {
                        if (item->objectName() == QStringLiteral("photoThumbnail")) {
                            const auto url = item->property("source").toUrl().toString();
                            if ((url == current ||
                                 url.startsWith(current + QStringLiteral("&retry="))) &&
                                item->property("status").toInt() == 1)
                                return true;
                        }
                        for (auto *child : item->childItems())
                            if (ready(child))
                                return true;
                        return false;
                    };
                    if (!ready(window->contentItem()))
                        return;
                }
                state->phase = 10;
                state->originalID = editor.photoID();
                library.createVirtualCopy(state->originalID,
                                          QStringLiteral("Library acceptance copy"));
                break;
            case 10:
                if (library.count() != 3)
                    return;
                for (int i = 0; i < library.count(); ++i) {
                    if (library.data(library.index(i), LibraryModel::CopyName).toString() ==
                        QStringLiteral("Library acceptance copy")) {
                        state->phase = 11;
                        library.activate(i);
                        break;
                    }
                }
                break;
            case 11:
                if (editor.busy() || editor.photoID() == state->originalID)
                    return;
                if (!editor.hasPhoto() || editor.currentSettings().exposure != 1.75 ||
                    !editor.filename().endsWith(QStringLiteral("Library acceptance copy"))) {
                    std::cerr << "The virtual copy did not open with its own Library identity\n";
                    app.exit(1);
                    return;
                }
                state->phase = 12;
                std::cout << "Library GUI acceptance passed\n";
                window->close();
                break;
            default:
                break;
            }
        });
    timer->start();
}
