#include "apps/shared/MetadataService.h"
#include "platform/macos/FileHasher.h"
#include "platform/macos/PhotoMetadataIO.h"
#include "tests/ux/UxContext.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSignalSpy>
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
} // namespace
bool metadataScenario(Context &c, const QString &id) {
    const bool catalogOnly = id == "UX-P09-META-002" || id == "UX-P09-META-007";
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
    require(!inputs.empty(), "Metadata case needs one public PNG");
    const auto source = std::filesystem::path((sourceFolder + "/Фото 夜景.png").toStdString());
    require(QFile::copy(c.fixture + '/' + inputs.front(), QString::fromStdString(source.string())),
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
