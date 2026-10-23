#include "apps/shared/MetadataService.h"
#include "platform/macos/FileHasher.h"
#include "platform/macos/PhotoMetadataIO.h"
#include "tests/ux/QueuedCallGate.h"
#include "tests/ux/UxContext.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTimer>
#include <algorithm>
#include <span>

namespace ux {
namespace {
void idle(Context &c) {
    c.driver->wait(
        [&] { return !c.library.working() && !c.library.loading() && !c.library.selecting(); },
        "Metadata worker and Library settled");
}
void reveal(Context &c, const QString &id) {
    auto &d = *c.driver;
    auto *target = d.item(id);
    auto *view = qobject_cast<QQuickItem *>(
        qvariant_cast<QObject *>(d.object("metadata.scroll")->property("contentItem")));
    require(view, "Metadata ScrollView has no Flickable");
    for (int attempt = 0; attempt < 80; ++attempt) {
        d.wait([&] { return !view->property("moving").toBool(); }, "Metadata wheel settled");
        d.frame();
        const auto rect = target->mapRectToItem(view, target->boundingRect());
        if (target->isVisible() && rect.top() >= 0 && rect.bottom() <= view->height())
            return;
        d.wheel("metadata.scroll", rect.top() < 0 ? 120 : -120);
    }
    throw std::runtime_error("Cannot reveal metadata control: " + id.toStdString());
}
void click(Context &c, const QString &id) {
    reveal(c, id);
    c.driver->click(id);
}
void open(Context &c, const std::string &photo) {
    auto &d = *c.driver;
    d.click("library.photoActions");
    d.click("library.photoMetadata");
    d.wait(
        [&] {
            return d.object("photoMetadata")->property("opened").toBool() &&
                   d.object("photoMetadata")->property("ready").toBool() &&
                   c.library.descriptiveMetadata().value("id").toString().toStdString() == photo;
        },
        "Photo metadata opened and populated asynchronously");
    require(c.library.error().isEmpty(), c.library.error());
}
void close(Context &c) {
    c.driver->key(Qt::Key_Escape);
    c.driver->wait([&] { return !c.driver->object("photoMetadata")->property("visible").toBool(); },
                   "Metadata dialog dismissed");
}
luma::DescriptiveMetadata unicodeValues() {
    luma::DescriptiveMetadata value;
    value.title = "Север <b>夜景</b> & լույս";
    value.caption = "Первая строка 🌅\nZweite Zeile — café";
    value.creators = {"Անի Սարգսյան", "山田 太郎"};
    value.copyright = "© 2026 Luma — автор";
    value.location = "Մեղրի / 北京";
    value.keywords = {"ночь", "빛", "café"};
    value.rating = 4;
    value.flag = 1;
    value.label = "Свой цвет";
    return value;
}
QString lines(const std::vector<std::string> &values) {
    QStringList result;
    for (const auto &value : values)
        result.append(QString::fromStdString(value));
    return result.join('\n');
}
void form(Context &c, const luma::DescriptiveMetadata &value, bool enter) {
    const std::pair<QString, QString> fields[]{
        {"title", QString::fromStdString(value.title)},
        {"caption", QString::fromStdString(value.caption)},
        {"creators", lines(value.creators)},
        {"copyright", QString::fromStdString(value.copyright)},
        {"location", QString::fromStdString(value.location)},
        {"keywords", lines(value.keywords)},
        {"label", QString::fromStdString(value.label)}};
    for (const auto &[name, text] : fields) {
        const auto id = "metadata." + name;
        if (enter) {
            reveal(c, id);
            c.driver->type(id, text);
            c.driver->log({{"action", "metadata_literal_input"},
                           {"id", id},
                           {"actual", c.driver->object(id)->property("text").toString()},
                           {"expected", text}});
        }
        require(c.driver->object(id)->property("text").toString() == text,
                "Metadata field did not retain literal Unicode/multiline text: " + id);
    }
    for (const auto &[id, index] : {std::pair{QStringLiteral("metadata.rating"), value.rating},
                                    std::pair{QStringLiteral("metadata.flag"), value.flag + 1}}) {
        if (enter) {
            reveal(c, id);
            c.driver->choose(id, index);
        }
        require(c.driver->object(id)->property("currentIndex").toInt() == index,
                "Metadata rating/flag differs from saved values");
    }
}
QByteArray read(const std::filesystem::path &path) {
    QFile file(QString::fromStdString(path.string()));
    require(file.open(QIODevice::ReadOnly), "Cannot read owned sidecar fixture");
    return file.readAll();
}
void write(const std::filesystem::path &path, const QByteArray &bytes) {
    QSaveFile file(QString::fromStdString(path.string()));
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit(),
            "Cannot publish owned external XMP fixture");
}
void readConfirmation(Context &c, bool accept) {
    auto &d = *c.driver;
    click(c, "metadata.read");
    d.wait([&] { return d.object("metadata.confirmRead")->property("opened").toBool(); },
           "Explicit replace-metadata confirmation opened");
    if (accept) {
        auto *footer =
            qvariant_cast<QObject *>(d.object("metadata.confirmRead")->property("footer"));
        require(footer, "Metadata confirmation has no standard button box");
        QQuickItem *ok = nullptr;
        for (auto *item : footer->findChildren<QQuickItem *>()) {
            if (item->inherits("QQuickAbstractButton") &&
                item->property("text").toString() == "OK" && item->isVisible() &&
                item->isEnabled()) {
                require(!ok, "Metadata confirmation has ambiguous OK controls");
                ok = item;
            }
        }
        require(ok, "Metadata confirmation has no actual OK button");
        ok->setObjectName("metadata.confirmRead.ok");
        d.click(ok->objectName());
    } else
        d.key(Qt::Key_Escape);
    d.wait([&] { return !d.object("metadata.confirmRead")->property("visible").toBool(); },
           "Metadata confirmation closed through user input");
    idle(c);
}
void captureMetadata(Context &c) {
    // Known Lens.DNG values; LibRaw normalizes Nikon make/model on decode.
    // Expectations are independent of the controller and QML bindings.
    const std::pair<QString, QString> fields[]{{"camera", "Nikon D7500"},
                                               {"lens", "AF-S DX Nikkor 18-140mm f/3.5-5.6G ED VR"},
                                               {"iso", "100"},
                                               {"shutter", "1/125 s"},
                                               {"aperture", "f/5.6"},
                                               {"focalLength", "18 mm"}};
    for (const auto &[name, expected] : fields) {
        const auto id = "develop.metadata." + name;
        c.reveal(id);
        auto *row = c.driver->item(id);
        require(row->isVisible() && row->property("value").toString() == expected,
                "Capture metadata is missing or incorrect: " + name + " = " +
                    row->property("value").toString());
        bool rendered = false;
        for (const auto *child : row->childItems())
            if (child->isVisible() && child->property("text").toString() == expected &&
                child->width() > 0 && child->height() > 0)
                rendered = true;
        require(rendered, "Capture metadata value is not bound to a visible Label: " + name);
        c.driver->log(
            {{"action", "capture_metadata_displayed"}, {"field", name}, {"expected", expected}});
    }
}
void pendingMetadata(Context &c, const std::string &photo) {
    auto &d = *c.driver;
    QSignalSpy changed(&c.library, &LibraryModel::metadataChanged);
    require(changed.isValid(), "Cannot observe metadata result delivery");
    QueuedCallGate gate(
        c.library,
        [&] {
            return c.library.descriptiveMetadata().isEmpty() &&
                   d.object("photoMetadata")->property("opened").toBool();
        },
        [&] {
            require(changed.count() == 1 &&
                        !d.object("photoMetadata")->property("ready").toBool() &&
                        !d.item("metadata.save")->isEnabled(),
                    "Pending metadata read exposed an editable stale draft");
            close(c);
            d.click("main.developButton");
            require(!c.window->property("libraryMode").toBool(),
                    "Pending metadata read blocked Develop navigation");
            d.click("main.libraryButton");
            d.click("library.clearSelection");
            require(c.library.selectedCount() == 0,
                    "Pending metadata read blocked clear selection");
            d.click(c.card(0));
            require(c.library.selectionIDs() == std::vector<std::string>{photo},
                    "Pending metadata read blocked Library selection");
            int beats = 0;
            QTimer timer;
            QObject::connect(&timer, &QTimer::timeout, [&] { ++beats; });
            timer.start(10);
            d.wait([&] { return beats >= 5; }, "GUI heartbeat while metadata completion is held");
            require(c.library.descriptiveMetadata().isEmpty() && changed.count() == 1 &&
                        c.window->property("libraryMode").toBool(),
                    "Fixture did not hold the actual metadata completion across Library input");
            d.log({{"action", "metadata_completion_held_across_library_input"},
                   {"heartbeat_events", beats},
                   {"completion_boundary_fixture", true}});
        });
    d.click("library.photoActions");
    d.click("library.photoMetadata");
    d.wait([&] { return gate.caught; }, "Metadata completion boundary intercepted");
    if (gate.failure)
        std::rethrow_exception(gate.failure);
    d.wait(
        [&] {
            return c.library.descriptiveMetadata().value("id").toString().toStdString() == photo;
        },
        "Original metadata callback delivered after Library input");
    require(changed.count() == 2 && !d.object("photoMetadata")->property("visible").toBool(),
            "Late metadata completion reopened the dialog or populated another result");
    idle(c);
}
void batchMetadata(Context &c) {
    auto &d = *c.driver;
    c.import(4);
    d.doubleClick(c.card(0));
    c.loaded();
    c.reveal("develop.exposureSlider");
    d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {1., .5});
    c.saved();
    d.click("main.libraryButton");
    d.click(c.card(0));
    d.click(c.card(1), Qt::ControlModifier);
    d.click(c.card(2), Qt::ControlModifier);
    const auto selected = c.library.selectionIDs();
    require(selected.size() == 3 && !c.library.autoWriteXMP(),
            "Batch metadata fixture needs three selected assets and catalog-only mode");
    luma::Catalog catalog(c.catalogFile.toStdString());
    const auto rows = catalog.libraryPage({}, 0, 4);
    require(rows.size() == 4, "Batch metadata requires an unselected control asset");
    std::vector<luma::AssetSnapshot> documents;
    for (const auto &row : rows)
        documents.push_back(catalog.assetSnapshot(row.id).value());
    QSignalSpy saves(&c.editor, &EditorController::editsSaved);
    require(saves.isValid(), "Cannot observe Develop saves during batch metadata changes");
    auto snapshots = [&] {
        std::vector<luma::MetadataSnapshot> values;
        for (const auto &row : rows)
            values.push_back(catalog.metadataSnapshot(row.id));
        return values;
    };
    auto unchanged = [&](const auto &before) {
        const auto after = snapshots();
        for (std::size_t i = 0; i < before.size(); ++i)
            require(after[i].values == before[i].values &&
                        after[i].revision == before[i].revision &&
                        after[i].writtenRevision == before[i].writtenRevision &&
                        after[i].sidecarHash == before[i].sidecarHash,
                    "Uncommitted/failed batch exposed partial metadata or revision changes");
    };
    struct ReleaseFault {
        SqliteFault &fault;
        ~ReleaseFault() {
            fault.diskFull(false);
            fault.holdWrites(false);
        }
    };
    for (int field = 0; field < 3; ++field) {
        auto invoke = [&] {
            if (field == 0)
                d.click("library.rate.4");
            else
                d.choose(field == 1 ? "library.flagChoice" : "library.labelChoice",
                         field == 1 ? 1 : 2);
        };
        const auto before = snapshots();
        for (const bool fail : {true, false}) {
            const int rejected = c.storageFault.rejectedWrites();
            {
                ReleaseFault release{c.storageFault};
                c.storageFault.holdWrites(true);
                invoke();
                d.wait([&] { return c.storageFault.writeBlocked(); },
                       "Real catalog write held after its first successful VFS write");
                require(c.library.working() && !d.item("library.metadataActions")->isEnabled(),
                        "Pending batch metadata write did not disable duplicate mutations");
                unchanged(before); // Independent SQLite connection cannot see a partial batch.
                for (std::size_t i = 0; i < rows.size(); ++i) {
                    const auto *card = d.object(c.card(static_cast<int>(i)));
                    require(card->property("rating").toInt() == before[i].values.rating &&
                                card->property("flagValue").toInt() == before[i].values.flag &&
                                card->property("colorLabel").toString().toStdString() ==
                                    before[i].values.colorLabel(),
                            "PhotoCard displayed uncommitted batch metadata");
                }
                if (fail)
                    c.storageFault.diskFull(true);
                c.storageFault.holdWrites(false);
                idle(c);
            }
            if (fail) {
                require(c.storageFault.rejectedWrites() > rejected &&
                            !c.library.error().isEmpty() && d.item("library.error")->isVisible() &&
                            d.object("library.error")->property("text").toString() ==
                                c.library.error(),
                        "Injected partial-write SQLITE_FULL did not produce a visible failure");
                unchanged(before);
            } else {
                require(c.library.error().isEmpty(), c.library.error());
                const auto after = snapshots();
                for (std::size_t i = 0; i < rows.size(); ++i) {
                    auto expected = before[i].values;
                    const bool chosen =
                        std::find(selected.begin(), selected.end(), rows[i].id) != selected.end();
                    if (chosen) {
                        if (field == 0)
                            expected.rating = 4;
                        else if (field == 1)
                            expected.flag = 1;
                        else
                            expected.label = "Red";
                    }
                    require(after[i].values == expected &&
                                after[i].revision == before[i].revision + (chosen ? 1 : 0) &&
                                after[i].writtenRevision == before[i].writtenRevision &&
                                after[i].sidecarHash == before[i].sidecarHash,
                            "Retry did not commit the complete selected batch exactly once");
                    const auto *card = d.object(c.card(static_cast<int>(i)));
                    require(card->property("rating").toInt() == expected.rating &&
                                card->property("flagValue").toInt() == expected.flag &&
                                card->property("colorLabel").toString().toStdString() ==
                                    expected.colorLabel(),
                            "PhotoCard did not display committed batch values");
                }
            }
            require(c.library.selectionIDs() == selected,
                    "Batch metadata changed selection identity");
            d.log({{"action", "batch_metadata_transaction"},
                   {"field", field},
                   {"injected_disk_full", fail},
                   {"selected_assets", 3},
                   {"control_assets", 1},
                   {"rejected_writes", c.storageFault.rejectedWrites() - rejected}});
        }
    }
    for (const auto &before : documents) {
        const auto after = catalog.assetSnapshot(before.asset.id).value();
        require(after.asset.edits == before.asset.edits && after.revision == before.revision &&
                    luma::macos::sha256File(after.asset.path) == before.asset.checksum &&
                    !std::filesystem::exists(luma::MetadataService::sidecarPath(after.asset.path)),
                "Batch metadata changed originals, Develop documents or sidecars");
    }
    require(saves.isEmpty() && c.editor.canUndo() && !c.editor.canRedo() && c.warnings.isEmpty(),
            "Batch metadata changed Develop history or emitted QML warnings");
    catalog.verify();
}
} // namespace
bool metadataScenario(Context &c, const QString &id) {
    if (id == "UX-P09-META-006") {
        batchMetadata(c);
        return true;
    }
    const bool complete = id == "UX-P09-META-001";
    const bool catalogOnly = complete || id == "UX-P09-META-002" || id == "UX-P09-META-007";
    const bool sidecarWrite = id == "UX-P09-XMP-002";
    const bool sidecarRead = id == "UX-P09-XMP-003";
    const bool conflict = id == "UX-P09-XMP-004";
    const bool malformed = id == "UX-P09-XMP-005";
    if (!catalogOnly && !sidecarWrite && !sidecarRead && !conflict && !malformed)
        return false;
    auto &d = *c.driver;
    require(c.localization->language() == "en", "Standard-button fixture requires English");
    const auto sourceFolder = c.root + "/metadata-originals";
    require(QDir().mkpath(sourceFolder), "Cannot create owned metadata input directory");
    const auto inputs = QDir(c.fixture).entryList({"*.png"}, QDir::Files, QDir::Name);
    require(complete || !inputs.empty(), "Metadata case needs one public PNG");
    const auto source = std::filesystem::path(
        (sourceFolder + (complete ? "/Фото 夜景.DNG" : "/Фото 夜景.png")).toStdString());
    require(
        QFile::copy(complete ? c.fixture + "/stress/Lens.DNG" : c.fixture + '/' + inputs.front(),
                    QString::fromStdString(source.string())),
        "Cannot copy owned metadata input");
    const auto sourceHash = luma::macos::sha256File(source);
    const auto sidecar = luma::MetadataService::sidecarPath(source);
    require(!std::filesystem::exists(sidecar) && !c.library.autoWriteXMP(),
            "Metadata baseline must use catalog-only storage without an existing sidecar");
    require(d.object("library.importDialog")
                ->setProperty("sources", QVariant::fromValue(QList<QUrl>{QUrl::fromLocalFile(
                                             QString::fromStdString(source.string()))})),
            "Declared file-chooser fixture was rejected");
    d.log({{"action", "fixture_file_chooser_result"}, {"count", 1}});
    d.click("library.importButton");
    d.wait([&] { return d.object("library.importDialog")->property("opened").toBool(); },
           "Owned metadata source Import opened");
    d.click("library.importStart");
    idle(c);
    require(c.library.error().isEmpty() && c.library.count() == 1,
            "Metadata source import did not commit");
    d.doubleClick(c.card(0));
    c.loaded();
    if (complete)
        captureMetadata(c);
    c.reveal("develop.exposureSlider");
    d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {1., .5});
    c.saved();
    const auto photo = c.editor.photoID().toStdString();
    luma::Catalog catalog(c.catalogFile.toStdString());
    const auto expectedEdits = catalog.assetSnapshot(photo).value();
    require(expectedEdits.asset.edits.exposure == 5 && c.editor.canUndo() && !c.editor.canRedo(),
            "Metadata fixture requires one saved Develop edit");
    QSignalSpy saves(&c.editor, &EditorController::editsSaved);
    require(saves.isValid(), "Cannot observe unexpected Develop saves during metadata work");
    d.click("main.libraryButton");
    d.click(c.card(0));
    const auto selected = c.library.selectionIDs();
    open(c, photo);
    auto expected = unicodeValues();
    form(c, expected, true);
    click(c, "metadata.save");
    idle(c);
    d.wait([&] { return d.object("photoMetadata")->property("ready").toBool(); },
           "Saved metadata draft reloaded");
    require(c.library.error().isEmpty() && catalog.metadataSnapshot(photo).values == expected &&
                !std::filesystem::exists(sidecar),
            "Catalog-only save lost values or silently created XMP");
    const auto saved = catalog.metadataSnapshot(photo);
    close(c);
    if (complete)
        pendingMetadata(c, photo);
    open(c, photo);
    form(c, expected, false);

    if (sidecarWrite) {
        click(c, "metadata.write");
        idle(c);
        require(c.library.error().isEmpty() && std::filesystem::is_regular_file(sidecar),
                "Explicit sidecar write failed");
        const auto bytes = read(sidecar);
        auto serializable = expected;
        serializable.flag = 0; // Pick is catalog-only; XMP stores a star rating here.
        const auto decoded = luma::macos::readDescriptiveMetadata(
            source,
            std::span(reinterpret_cast<const std::uint8_t *>(bytes.constData()), bytes.size()));
        const auto written = catalog.metadataSnapshot(photo);
        require(decoded == serializable && written.values == expected &&
                    written.writtenRevision == written.revision,
                "Explicit XMP write lost supported Unicode fields or its durable acknowledgement");
    } else if (sidecarRead || conflict || malformed) {
        const QByteArray external =
            malformed ? QByteArray("<x:xmpmeta>broken & unclosed") : QByteArray(R"xml(
<x:xmpmeta xmlns:x="adobe:ns:meta/"><rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
<rdf:Description rdf:about="" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:xmp="http://ns.adobe.com/xap/1.0/" xmp:Rating="3" xmp:Label="External">
<dc:title><rdf:Alt><rdf:li xml:lang="x-default">Внешний 夜景 &amp; café</rdf:li></rdf:Alt></dc:title>
<dc:description><rdf:Alt><rdf:li xml:lang="x-default">Отдельный XMP</rdf:li></rdf:Alt></dc:description>
<dc:creator><rdf:Seq><rdf:li>Անի</rdf:li></rdf:Seq></dc:creator>
<dc:subject><rdf:Bag><rdf:li>外部</rdf:li></rdf:Bag></dc:subject>
</rdf:Description></rdf:RDF></x:xmpmeta>)xml");
        write(sidecar, external); // External writer boundary, private per-case source only.
        if (conflict) {
            click(c, "metadata.write");
            idle(c);
            require(c.library.error().contains("XMP changed outside Luma") &&
                        c.library.error().contains("Import its metadata before writing"),
                    "Changed external XMP did not produce explicit conflict/recovery instructions");
        } else {
            readConfirmation(c, false);
            require(catalog.metadataSnapshot(photo).values == saved.values &&
                        catalog.metadataSnapshot(photo).revision == saved.revision,
                    "Cancel on import-metadata confirmation changed saved values");
            form(c, expected, false);
            readConfirmation(c, true);
        }
        if (malformed || conflict) {
            require(!c.library.error().isEmpty(), "Invalid external packet was silently accepted");
            reveal(c, "metadata.error");
            require(d.object("metadata.error")->property("text").toString() == c.library.error(),
                    "Metadata failure is not shown in the actual dialog");
            const auto after = catalog.metadataSnapshot(photo);
            require(after.values == saved.values && after.revision == saved.revision &&
                        after.sidecarHash == saved.sidecarHash &&
                        after.writtenRevision == saved.writtenRevision,
                    "Failed XMP operation partially changed catalog metadata or acknowledgement");
        } else {
            expected = {};
            expected.title = "Внешний 夜景 & café";
            expected.caption = "Отдельный XMP";
            expected.creators = {"Անի"};
            expected.keywords = {"外部"};
            expected.rating = 3;
            expected.label = "External";
            d.wait([&] { return d.object("photoMetadata")->property("ready").toBool(); },
                   "Imported external metadata populated the dialog");
            require(c.library.error().isEmpty() &&
                        catalog.metadataSnapshot(photo).values == expected,
                    "Existing XMP was not explicitly imported into the catalog");
            form(c, expected, false);
        }
        require(read(sidecar) == external,
                "Read/conflict/error workflow changed external XMP bytes");
    }
    close(c);
    const auto after = catalog.assetSnapshot(photo).value();
    require(after.asset.edits == expectedEdits.asset.edits &&
                after.revision == expectedEdits.revision &&
                c.editor.currentSettings() == expectedEdits.asset.edits && saves.isEmpty() &&
                c.editor.canUndo() && !c.editor.canRedo() && c.library.selectionIDs() == selected &&
                c.editor.photoID().toStdString() == photo &&
                luma::macos::sha256File(source) == sourceHash,
            "Metadata workflow changed the original, Develop edits, history or selected identity");
    if (catalogOnly)
        require(!std::filesystem::exists(sidecar),
                "Catalog-only workflow silently wrote a sidecar");
    catalog.verify();
    require(c.warnings.isEmpty(),
            "Metadata workflow produced QML warnings: " + c.warnings.join('\n'));
    d.log({{"action", "metadata_workflow_preserved_data"},
           {"case", id},
           {"original_sha256", QString::fromStdString(sourceHash)},
           {"develop_revision", static_cast<qint64>(after.revision)},
           {"metadata_revision", static_cast<qint64>(catalog.metadataSnapshot(photo).revision)}});
    return true;
}
} // namespace ux
