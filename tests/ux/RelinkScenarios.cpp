#include "apps/shared/MetadataService.h"
#include "platform/macos/FileHasher.h"
#include "tests/ux/UxContext.h"
#include "tests/ux/UxPixels.h"
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <algorithm>

namespace ux {
namespace {
void idle(Context &c) {
    c.driver->wait(
        [&] { return !c.library.working() && !c.library.loading() && !c.library.selecting(); },
        "Relink Library workers settled");
}
QString card(Context &c, const std::string &photo) {
    for (int row = 0; row < c.library.count(); ++row) {
        const auto item = c.card(row);
        if (c.driver->object(item)->property("assetID").toString().toStdString() == photo)
            return item;
    }
    throw std::runtime_error("Relink fixture lost an asset from the grid");
}
void reveal(Context &c, const QString &id) {
    auto &d = *c.driver;
    auto *target = d.item(id);
    auto *view = qobject_cast<QQuickItem *>(
        qvariant_cast<QObject *>(d.object("metadata.scroll")->property("contentItem")));
    require(view, "Metadata ScrollView has no Flickable");
    for (int attempt = 0; attempt < 80; ++attempt) {
        d.wait([&] { return !view->property("moving").toBool(); }, "Relink metadata wheel settled");
        d.frame();
        const auto rect = target->mapRectToItem(view, target->boundingRect());
        if (target->isVisible() && rect.top() >= 0 && rect.bottom() <= view->height())
            return;
        d.wheel("metadata.scroll", rect.top() < 0 ? 120 : -120);
    }
    throw std::runtime_error("Cannot reveal relink control: " + id.toStdString());
}
void metadata(Context &c, const std::string &photo) {
    auto &d = *c.driver;
    d.click("library.photoActions");
    d.click("library.photoMetadata");
    d.wait(
        [&] {
            return d.object("photoMetadata")->property("opened").toBool() &&
                   d.object("photoMetadata")->property("ready").toBool() &&
                   c.library.descriptiveMetadata().value("id").toString().toStdString() == photo;
        },
        "Relink metadata draft ready");
}
void closeMetadata(Context &c) {
    auto *footer = qvariant_cast<QObject *>(c.driver->object("photoMetadata")->property("footer"));
    require(footer, "Metadata dialog has no standard button box");
    QQuickItem *close = nullptr;
    for (auto *item : footer->findChildren<QQuickItem *>()) {
        if (item->inherits("QQuickAbstractButton") &&
            item->property("text").toString() == "Close" && item->isVisible() &&
            item->isEnabled()) {
            require(!close, "Metadata dialog has ambiguous Close controls");
            close = item;
        }
    }
    require(close, "Metadata dialog has no actual Close button");
    close->setObjectName("metadata.close");
    c.driver->click(close->objectName());
    c.driver->wait([&] { return !c.driver->object("photoMetadata")->property("visible").toBool(); },
                   "Relink metadata dialog dismissed");
}
void locate(Context &c, const std::filesystem::path &path, bool folder) {
    auto &d = *c.driver;
    const auto button = folder ? "metadata.locateFolder" : "metadata.locateOriginal";
    const auto dialogID = folder ? "metadata.locateFolderDialog" : "metadata.locateFileDialog";
    const auto property = folder ? "selectedFolder" : "selectedFile";
    reveal(c, button);
    d.click(button);
    auto *dialog = d.object(dialogID);
    d.wait([&] { return dialog->property("visible").toBool(); },
           "Actual Locate control opened chooser");
    const auto url = QUrl::fromLocalFile(QString::fromStdString(path.string()));
    require(dialog->setProperty(property, url) && dialog->property(property).toUrl() == url,
            "Locate chooser rejected declared owned-path fixture");
    d.log({{"action", "file_dialog_selection_fixture_boundary"},
           {"id", dialogID},
           {"path", url.toLocalFile()},
           {"native_selection_exercised", false}});
    require(QMetaObject::invokeMethod(dialog, "close", Qt::DirectConnection) &&
                QMetaObject::invokeMethod(dialog, "accepted", Qt::DirectConnection),
            "Locate dialog lacks its production close/accepted handlers");
    d.wait([&] { return !dialog->property("visible").toBool(); }, "Locate chooser closed");
    idle(c);
}
void missing(Context &c, const std::string &photo, bool expected) {
    auto &d = *c.driver;
    d.wait([&] { return d.object(card(c, photo))->property("sourceMissing").toBool() == expected; },
           (expected ? "Filesystem watcher marks missing original: "
                     : "Relink clears missing original: ") +
               QString::fromStdString(photo));
    if (expected) {
        auto *badge = d.item("library.availability." + QString::fromStdString(photo));
        require(badge->isVisible() && badge->property("text").toString() == "Missing",
                "Missing original is not visibly marked on its PhotoCard");
    }
}
} // namespace
bool relinkScenario(Context &c, const QString &id) {
    const bool absent = id == "UX-P09-RLK-001";
    const bool tree = id == "UX-P09-RLK-003";
    const bool wrong = id == "UX-P09-RLK-004";
    if (!absent && !tree && !wrong && id != "UX-P09-RLK-002" && id != "UX-P09-RLK-006" &&
        id != "UX-P09-RLK-007")
        return false;
    auto &d = *c.driver;
    require(c.localization->language() == "en", "Relink literals require English");
    const auto root = std::filesystem::canonical(c.root.toStdString());
    const auto oldRoot = root / "relink-originals", newRoot = root / "moved-originals";
    std::filesystem::create_directories(oldRoot / "nested");
    std::filesystem::create_directory(root / "control-originals");
    const std::vector<std::filesystem::path> sources{oldRoot / "A-Фото.png",
                                                     oldRoot / "nested/B-夜景.png",
                                                     root / "control-originals/C-control.png"};
    const auto inputs = QDir(c.fixture).entryList({"*.png"}, QDir::Files, QDir::Name);
    require(inputs.size() >= 3, "Relink requires three public source fixtures");
    QList<QUrl> urls;
    for (int i = 0; i < 3; ++i) {
        require(
            QFile::copy(c.fixture + '/' + inputs[i], QString::fromStdString(sources[i].string())),
            "Cannot copy owned relink original");
        urls.append(QUrl::fromLocalFile(QString::fromStdString(sources[i].string())));
    }
    require(d.object("library.importDialog")->setProperty("sources", QVariant::fromValue(urls)),
            "Import rejected relink input fixture");
    d.click("library.importButton");
    d.wait([&] { return d.object("library.importDialog")->property("opened").toBool(); },
           "Relink Import opened");
    d.click("library.importStart");
    idle(c);
    require(c.library.error().isEmpty() && c.library.count() == 3,
            "Relink sources were not imported");
    luma::Catalog catalog(c.catalogFile.toStdString());
    const auto rows = catalog.libraryPage({}, 0, 3);
    require(rows.size() == 3 && rows[0].path == sources[0] && rows[1].path == sources[1] &&
                rows[2].path == sources[2],
            "Relink fixture must have known identity/order before moving originals");
    const auto photo = rows[0].id;
    d.doubleClick(card(c, photo));
    c.loaded();
    c.reveal("develop.exposureSlider");
    d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {.6, .5});
    c.saved();
    checkPreviewPixelOracle();
    const auto expectedPreview = fixtureReference(c);
    d.wait(
        [&] {
            return previewPixelsAgree(c.provider->requestImage({}, nullptr, {}), expectedPreview,
                                      c.editor.metalPreview());
        },
        "Edited original preview matches independent CPU reference");
    d.click("main.libraryButton");
    d.click(card(c, photo));
    d.click(card(c, rows[1].id), Qt::ControlModifier);
    d.click("library.newCollection");
    d.wait([&] { return d.object("library.collectionDialog")->property("opened").toBool(); },
           "Relink collection opened");
    d.type("library.collectionName", "Relink · Հավաքածու");
    d.click("library.collectionConfirm");
    idle(c);
    require(c.library.collections().size() == 1, "Relink collection was not created");
    d.click("libraryAddToCollection");
    idle(c);
    const auto collection = catalog.collections().front();
    luma::LibraryFilter filter;
    filter.collectionID = collection.id;
    const auto members = catalog.libraryIDs(filter);
    require(members == std::vector<std::string>{rows[0].id, rows[1].id},
            "Relink collection lost its two selected members");
    d.click(card(c, photo));
    metadata(c, photo);
    for (const auto &[name, text] : {std::pair{"metadata.title", "Relink · 夜景"},
                                     std::pair{"metadata.caption", "Keep edits and identity"},
                                     std::pair{"metadata.keywords", "пейзаж\nլույս"}}) {
        reveal(c, name);
        d.type(name, QString::fromUtf8(text));
    }
    reveal(c, "metadata.rating");
    d.choose("metadata.rating", 4);
    reveal(c, "metadata.flag");
    d.choose("metadata.flag", 2);
    reveal(c, "metadata.label");
    d.type("metadata.label", "Red");
    reveal(c, "metadata.save");
    d.click("metadata.save");
    idle(c);
    d.wait([&] { return d.object("photoMetadata")->property("ready").toBool(); },
           "Relink metadata saved and reloaded");
    require(c.library.error().isEmpty(), c.library.error());
    closeMetadata(c);
    std::vector<luma::AssetSnapshot> documents;
    std::vector<luma::MetadataSnapshot> values;
    for (const auto &row : rows) {
        documents.push_back(catalog.assetSnapshot(row.id).value());
        values.push_back(catalog.metadataSnapshot(row.id));
    }
    require(documents[0].asset.edits.exposure != 0 && values[0].values.rating == 4 &&
                values[0].values.flag == 1 && values[0].values.label == "Red" &&
                c.editor.canUndo() && !c.editor.canRedo(),
            "Relink requires saved edits, descriptive metadata, rating, flag, label and history");
    const auto selection = c.library.selectionIDs();
    QSignalSpy saved(&c.editor, &EditorController::editsSaved);
    QSignalSpy opened(&c.library, &LibraryModel::openRequested);
    require(saved.isValid() && opened.isValid(), "Cannot observe relink document activity");
    auto destination = newRoot / "A-renamed.png";
    if (tree) {
        std::filesystem::rename(oldRoot, newRoot);
        destination = newRoot / sources[0].filename();
    } else {
        std::filesystem::create_directory(newRoot);
        std::filesystem::rename(sources[0], destination);
    }
    auto unchanged = [&](bool relinked) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const bool moved = relinked && (i == 0 || (tree && i == 1));
            const auto after = catalog.assetSnapshot(rows[i].id).value();
            const auto metadata = catalog.metadataSnapshot(rows[i].id);
            const auto path = !moved ? sources[i]
                              : tree ? newRoot / sources[i].lexically_relative(oldRoot)
                                     : destination;
            require(
                after.asset.path == path && after.asset.checksum == documents[i].asset.checksum &&
                    after.asset.edits == documents[i].asset.edits &&
                    after.revision == documents[i].revision &&
                    metadata.values == values[i].values &&
                    metadata.revision == values[i].revision + (moved ? 1 : 0) &&
                    metadata.writtenRevision == values[i].writtenRevision &&
                    metadata.sidecarHash == values[i].sidecarHash,
                "Relink changed a document, metadata values, identity or unexpected revision/path");
        }
        require(catalog.libraryIDs(filter) == members && c.library.selectionIDs() == selection &&
                    c.editor.photoID().toStdString() == photo &&
                    c.editor.currentSettings() == documents[0].asset.edits && saved.isEmpty() &&
                    c.editor.canUndo() && !c.editor.canRedo(),
                "Relink changed collection members, selection, active edits or history");
    };
    missing(c, photo, true);
    missing(c, rows[1].id, tree);
    missing(c, rows[2].id, false);
    unchanged(false);
    require(c.provider->requestImage({}, nullptr, {}).size() == expectedPreview.size() &&
                previewPixelsAgree(c.provider->requestImage({}, nullptr, {}), expectedPreview,
                                   c.editor.metalPreview()),
            "Missing-original notification changed the current edited preview");
    if (absent) {
        std::filesystem::rename(destination, sources[0]);
        missing(c, photo, false);
        unchanged(false);
        // Observe a parent-directory rename, its restoration, and a later file
        // change. The last move requires rearming watches removed by the OS.
        const auto movedTree = root / "temporarily-moved-tree";
        std::filesystem::rename(oldRoot, movedTree);
        missing(c, photo, true);
        missing(c, rows[1].id, true);
        missing(c, rows[2].id, false);
        unchanged(false);
        std::filesystem::rename(movedTree, oldRoot);
        missing(c, photo, false);
        missing(c, rows[1].id, false);
        std::filesystem::rename(sources[1], newRoot / sources[1].filename());
        missing(c, rows[1].id, true);
        missing(c, photo, false);
        std::filesystem::rename(newRoot / sources[1].filename(), sources[1]);
        missing(c, rows[1].id, false);
        unchanged(false);
    } else {
        metadata(c, photo);
        if (wrong) {
            const auto incorrect = root / "wrong-candidate" / sources[0].filename();
            std::filesystem::create_directory(incorrect.parent_path());
            require(QFile::copy(QString::fromStdString(destination.string()),
                                QString::fromStdString(incorrect.string())),
                    "Cannot prepare same-name/same-size wrong candidate");
            QFile file(QString::fromStdString(incorrect.string()));
            require(file.open(QIODevice::ReadWrite) && file.seek(64),
                    "Cannot open owned wrong candidate");
            const auto byte = file.read(1);
            require(byte.size() == 1 && file.seek(64) &&
                        file.write(QByteArray(1, static_cast<char>(byte[0] ^ 1))) == 1,
                    "Cannot alter checksum in owned same-size candidate");
            file.close();
            const auto wrongHash = luma::macos::sha256File(incorrect);
            require(std::filesystem::file_size(incorrect) ==
                            std::filesystem::file_size(destination) &&
                        wrongHash != documents[0].asset.checksum,
                    "Wrong candidate must differ by checksum, not file size");
            locate(c, incorrect, false);
            require(c.library.error().contains("does not match the original checksum and size"),
                    "Wrong candidate did not show checksum refusal");
            reveal(c, "metadata.error");
            require(d.item("metadata.error")->isVisible() &&
                        d.object("metadata.error")->property("text").toString() ==
                            c.library.error(),
                    "Checksum refusal is not visible in metadata dialog");
            unchanged(false);
            missing(c, photo, true);
            require(luma::macos::sha256File(incorrect) == wrongHash,
                    "Refusal changed the rejected candidate");
        }
        locate(c, tree ? newRoot : destination, tree);
        require(c.library.error().isEmpty(), c.library.error());
        d.wait(
            [&] {
                return d.object("photoMetadata")->property("ready").toBool() &&
                       d.object("metadata.path")->property("text").toString().toStdString() ==
                           destination.string();
            },
            "Metadata dialog displays the committed new original path");
        unchanged(true);
        closeMetadata(c);
        for (const auto &row : rows)
            missing(c, row.id, false);
        d.doubleClick(card(c, photo));
        d.wait(
            [&] {
                return opened.count() == 1 &&
                       c.editor.sourcePath().toStdString() == destination.string() &&
                       !c.editor.busy() && !c.editor.previewUrl().isEmpty();
            },
            "Reopen loads the relinked original path");
        require(opened.front().front().toUrl().toLocalFile().toStdString() ==
                        destination.string() &&
                    c.editor.error().isEmpty(),
                "Library activated an obsolete path after relinking");
        d.wait(
            [&] {
                return previewPixelsAgree(c.provider->requestImage({}, nullptr, {}),
                                          expectedPreview, c.editor.metalPreview());
            },
            "Relinked original regenerates the saved edited preview");
        // Reopening reconstructs edit history from the catalog's durable document.
        require(c.editor.photoID().toStdString() == photo &&
                    c.editor.currentSettings() == documents[0].asset.edits && saved.isEmpty(),
                "Reopening relinked original lost its identity or saved edits");
    }
    for (std::size_t i = 0; i < sources.size(); ++i) {
        const auto record = catalog.fileRecord(rows[i].id);
        require(record.has_value(), "Relink lost an original file record");
        const auto path = record->path;
        require(luma::macos::sha256File(path) == documents[i].asset.checksum &&
                    !std::filesystem::exists(luma::MetadataService::sidecarPath(path)),
                "Relink changed original bytes or created an unsolicited sidecar");
    }
    require(c.warnings.isEmpty(), "Relink produced QML warnings: " + c.warnings.join('\n'));
    catalog.verify();
    d.log({{"action", "relink_preserved_saved_state"},
           {"case", id},
           {"moved_tree", tree},
           {"checksum_refusal_tested", wrong},
           {"originals", 3},
           {"collection_members", 2},
           {"native_picker_selection_exercised", false}});
    return true;
}
} // namespace ux
