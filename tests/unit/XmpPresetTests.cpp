#include "apps/desktop/EditorController.h"
#include "apps/desktop/PresetController.h"
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QThread>
#include <algorithm>
#include <functional>
#include <memory>

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename F> void rejects(F work) {
    bool rejected = false;
    try {
        work();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "Invalid XMP import was accepted.");
}
void write(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(),
            "Cannot prepare XMP fixture.");
}
QByteArray read(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read XMP fixture.");
    return file.readAll();
}
void wait(const std::function<bool()> &ready) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 30000) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
    require(ready(), "XMP editor acceptance timed out.");
}
QByteArray wrap(const QByteArray &attributes, const QByteArray &children = {}) {
    return "<x:xmpmeta xmlns:x='adobe:ns:meta/'><r:RDF "
           "xmlns:r='http://www.w3.org/1999/02/22-rdf-syntax-ns#'><r:Description "
           "xmlns:p='http://ns.adobe.com/camera-raw-settings/1.0/' " +
           attributes + ">" + children + "</r:Description></r:RDF></x:xmpmeta>";
}
void documentTests(const QString &root) {
    // Project-authored RDF fixtures; no third-party commercial preset data.
    const auto fixture =
        wrap("p:Exposure2012='+1.25' p:Exposure='-2' p:Temperature='5100' p:Tint='-12' "
             "p:WhiteBalance='Custom' p:HueAdjustmentRed='25' p:ColorNoiseReduction='35' "
             "p:SharpenRadius='1.8' p:Texture='20' p:CameraProfile='Adobe Color'",
             "<p:Name><r:Alt><r:li xml:lang='fr'>Autre nom</r:li>"
             "<r:li xml:lang='x-default'>Test &lt;b&gt;XMP&lt;/b&gt;</r:li></r:Alt></p:Name>"
             "<p:Group><r:Alt><r:li xml:lang='x-default'>Imported</r:li></r:Alt></p:Group>"
             "<p:ToneCurvePV2012Blue><r:Seq><r:li>0, 0</r:li><r:li>128, 150</r:li>"
             "<r:li>255, 255</r:li></r:Seq></p:ToneCurvePV2012Blue>"
             "<p:Look><p:Parameters p:Exposure2012='-4'/></p:Look>");
    const auto imported = luma::decodeXmpPreset(fixture, "Fallback");
    const auto &preset = imported.preset;
    require(preset.name == "Test <b>XMP</b>" && preset.folder == "Imported" &&
                preset.groups == 31 && preset.settings.exposure == 1.25 &&
                preset.settings.temperature == 5100 && preset.settings.tint == -12 &&
                preset.settings.customWhiteBalance && preset.settings.hsl[0].hue == 25 &&
                preset.settings.noiseColor == 35 && preset.settings.sharpenRadius == 1.8 &&
                preset.settings.curves[3].points[1].y == 150.0 / 255 &&
                imported.unsupported == QStringList{"CameraProfile", "Look", "Texture"},
            "XMP namespace, metadata, scalar, array or nested-resource mapping failed.");
    luma::EditDocument baseline;
    baseline.contrast = 43;
    baseline.saturation = 17;
    baseline.sharpenAmount = 80;
    baseline.noiseLuminance = 24;
    baseline.hsl[0].saturation = 15;
    baseline.curves[0].points = {{0, 0.1}, {1, 0.9}};
    baseline.setEnabled(luma::Correction::contrast, false);
    const auto applied =
        luma::mergeSettings(baseline, preset.settings, preset.groups, preset.parameters);
    require(applied.exposure == 1.25 && applied.contrast == baseline.contrast &&
                !applied.enabled(luma::Correction::contrast) &&
                applied.saturation == baseline.saturation && applied.hsl[0].hue == 25 &&
                applied.hsl[0].saturation == baseline.hsl[0].saturation &&
                applied.curves[0] == baseline.curves[0] &&
                applied.curves[3] == preset.settings.curves[3] &&
                applied.sharpenAmount == baseline.sharpenAmount &&
                applied.noiseLuminance == baseline.noiseLuminance,
            "Sparse XMP overwrote absent settings within a group.");
    const auto encoded = luma::encodePreset(preset);
    const auto decoded = luma::decodePreset(encoded);
    require(decoded.parameters == preset.parameters &&
                luma::mergeSettings(baseline, decoded.settings, decoded.groups,
                                    decoded.parameters) == applied,
            "Native JSON lost sparse XMP behavior.");
    luma::PresetStore store(root + "/xmp-store.json");
    const auto id = store.put(preset);
    require(store.load()[0].id == id && store.load()[0].preset.parameters == preset.parameters,
            "Sparse preset did not survive storage restart.");
    auto invalid = QJsonDocument::fromJson(encoded).object();
    invalid["parameters"] = QJsonArray{"exposure", "exposure"};
    rejects([&] { luma::decodePreset(QJsonDocument(invalid).toJson()); });
    invalid["parameters"] = QJsonArray{"crop.x"};
    rejects([&] { luma::decodePreset(QJsonDocument(invalid).toJson()); });
    invalid["parameters"] = QJsonArray{"exposure"};
    rejects([&] { luma::decodePreset(QJsonDocument(invalid).toJson()); });
    const auto element =
        luma::decodeXmpPreset(wrap({}, "<p:Exposure2012>0.5</p:Exposure2012>"), "Filename");
    require(element.preset.name == "Filename" && element.preset.settings.exposure == 0.5,
            "Element-form XMP setting was lost.");
    baseline.temperature = 8200;
    baseline.tint = 19;
    baseline.customWhiteBalance = true;
    const auto shot = luma::decodeXmpPreset(wrap("p:WhiteBalance='As Shot'"), "Shot").preset;
    const auto wb = luma::mergeSettings(baseline, shot.settings, shot.groups, shot.parameters);
    require(!wb.customWhiteBalance && wb.temperature == 8200 && wb.tint == 19,
            "As Shot XMP overwrote absent absolute WB values.");
    const auto simple = wrap("p:Exposure2012='0.7'");
    for (const auto &bad :
         {QByteArray("<!DOCTYPE x [<!ENTITY e SYSTEM 'file:///not-read'>]>") + simple,
          simple.left(simple.size() - 5), wrap("p:Exposure2012='nan'"), wrap("p:Exposure2012='99'"),
          wrap("p:Exposure2012='1,5'"), wrap("p:Temperature='50000'"),
          wrap("p:Exposure2012='1'", "<p:Exposure2012>2</p:Exposure2012>"),
          wrap("p:CameraProfile='Only unsupported data'"),
          QByteArray(simple).replace("camera-raw-settings/1.0/", "other/1.0/"),
          QByteArray(1'048'577, ' '),
          wrap({}, "<p:ToneCurvePV2012><r:Seq><r:li>0,0</r:li><r:li>255,300</r:li>"
                   "</r:Seq></p:ToneCurvePV2012>")})
        rejects([&] { luma::decodeXmpPreset(bad, "Bad"); });
    const auto ambiguous = QByteArray(simple).replace(
        "</r:RDF>", "<r:Description xmlns:p='http://ns.adobe.com/camera-raw-settings/1.0/' "
                    "p:Exposure2012='2'/></r:RDF>");
    rejects([&] { luma::decodeXmpPreset(ambiguous, "Bad"); });
    QByteArray nested;
    for (int i = 0; i < 33; ++i)
        nested += "<deep>";
    for (int i = 0; i < 33; ++i)
        nested += "</deep>";
    rejects([&] { luma::decodeXmpPreset(wrap("p:Exposure2012='1'", nested), "Bad"); });
    QByteArray many;
    for (int i = 0; i < 4100; ++i)
        many += "<item/>";
    rejects([&] { luma::decodeXmpPreset(wrap("p:Exposure2012='1'", many), "Bad"); });
    const auto file = root + "/Imported.XMP";
    write(file, fixture);
    require(luma::PresetStore::inspectImport(file).xmp, "Upper-case XMP extension was missed.");
}
} // namespace

void xmpPresetTests(const QString &source, const QString &root, const QString &qmlPath) {
    documentTests(root);
    const auto file = root + "/editor.xmp", collection = root + "/xmp-editor-presets.json";
    write(file, wrap("p:Exposure2012='1.3' p:Texture='30' p:CameraProfile='Adobe Color'",
                     "<p:Name><r:Alt><r:li xml:lang='x-default'>Imported test</r:li>"
                     "</r:Alt></p:Name>"));
    PreviewProvider provider;
    const auto catalog = root + "/xmp-editor.sqlite";
    EditorController editor(catalog.toStdString(), &provider);
    PresetController presets(collection, editor);
    editor.openFile(QUrl::fromLocalFile(source));
    wait([&] { return !editor.busy() && editor.hasPhoto() && !editor.previewUrl().isEmpty(); });
    editor.setParameter("contrast", 37);
    editor.setParameter("highlights", -25);
    editor.setParameter("saturation", 16);
    wait([&] { return !editor.saving(); });
    const auto baseline = editor.currentSettings();
    const auto importUrl = editor.previewUrl();
    require(presets.importPreset(QUrl::fromLocalFile(file)), "Controller refused XMP import.");
    require(editor.currentSettings() == baseline && editor.previewUrl() == importUrl &&
                !presets.importNotice().isEmpty() && presets.importUnsupported().size() == 2,
            "Import applied settings or lost compatibility report.");
    const auto id = presets.items()[0].toMap()["id"].toString();
    const auto beforeHover = editor.previewUrl();
    presets.preview(id);
    wait([&] { return editor.previewUrl() != beforeHover; });
    require(editor.currentSettings() == baseline && !editor.saving(), "XMP hover saved edits.");
    const auto hovered = provider.requestImage({}, nullptr, {});
    presets.clearPreview();
    presets.apply(id);
    auto expected = baseline;
    expected.exposure = 1.3;
    require(editor.currentSettings() == expected, "XMP apply reset absent Light settings.");
    wait([&] { return !editor.saving(); });
    require(luma::SessionService(catalog.toStdString()).restore()->asset.edits == expected,
            "XMP application did not survive catalog restart.");
    editor.undo();
    require(editor.currentSettings() == baseline, "XMP apply was not one Undo.");
    editor.redo();
    const auto beforeRender = editor.previewUrl();
    wait([&] { return !editor.saving() && editor.previewUrl() != beforeRender; });
    require(provider.requestImage({}, nullptr, {}) == hovered, "XMP hover/apply pixels differ.");
    const auto exportPath = root + "/xmp-export.json";
    require(presets.exportPreset(id, QUrl::fromLocalFile(exportPath)), "XMP JSON export failed.");
    require(luma::PresetStore::importFile(exportPath).parameters ==
                std::vector<std::string>{"exposure"},
            "Export/reimport expanded sparse settings.");
    require(presets.save(id, "Renamed XMP", "Imported", 1, false) &&
                presets.snapshot(id).parameters == std::vector<std::string>{"exposure"},
            "Renaming discarded sparse selection.");
    const auto stored = read(collection);
    write(file, "<invalid>");
    require(!presets.importPreset(QUrl::fromLocalFile(file)) && read(collection) == stored &&
                editor.currentSettings() == expected,
            "Failed XMP import modified storage or active edits.");
    write(file, wrap("p:Exposure2012='1.3' p:Texture='30'"));
    require(presets.importPreset(QUrl::fromLocalFile(file)), "Cannot restore import report.");
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("editor", &editor);
    engine.rootContext()->setContextProperty("presets", &presets);
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError> &errors) {
        for (const auto &error : errors)
            warnings.append(error.toString());
    });
    QQmlComponent component(&engine);
    component.setData(("import QtQuick\nimport QtQuick.Controls\nimport \"" +
                       QUrl::fromLocalFile(qmlPath).toString() +
                       "\"\nApplicationWindow { width: 400; height: 800; visible: true; "
                       "PresetPanel { anchors.fill: parent } }")
                          .toUtf8(),
                      QUrl());
    wait([&] { return component.status() != QQmlComponent::Loading; });
    require(!component.isError(), "XMP preset panel QML failed.");
    const std::unique_ptr<QObject> window(component.create());
    require(window != nullptr, "XMP QML window is missing.");
    auto *dialog = window->findChild<QObject *>("presetImportDialog");
    require(dialog && dialog->property("nameFilters").toStringList().join(' ').contains("*.xmp"),
            "Native import dialog hides XMP files.");
    auto *details = window->findChild<QObject *>("presetImportDetails");
    require(details && details->property("visible").toBool() &&
                QMetaObject::invokeMethod(details, "clicked"),
            "XMP compatibility details are inaccessible.");
    QCoreApplication::processEvents();
    require(window->findChild<QObject *>("xmpCompatibilityDialog")->property("visible").toBool() &&
                window->findChild<QObject *>("xmpUnsupportedList")->property("count").toInt() == 1,
            "XMP details dialog lost unsupported fields.");
    require(warnings.isEmpty(), "XMP UI emitted QML warnings.");
}
