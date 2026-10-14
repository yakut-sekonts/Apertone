import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: panel
    objectName: "exportPanel"
    title: "Экспорт"
    modal: true
    anchors.centerIn: parent
    width: Math.min(parent.width - 60, 650)
    height: Math.min(parent.height - 50, 790)
    property var form: exporter.settings
    function update(key, value) { const copy = Object.assign({}, form); copy[key] = value; form = copy }
    onOpened: { form = exporter.settings; presets.clearPreview() }
    Connections { target: exporter; function onSettingsChanged() { panel.form = exporter.settings } }
    FolderDialog { id: folder; title: "Папка экспорта"; onAccepted: panel.update("destination", exporter.localPath(selectedFolder)) }
    FileDialog { id: watermarkFile; title: "Изображение watermark"; nameFilters: ["Изображения (*.png *.jpg *.jpeg *.tif *.tiff)"]; onAccepted: panel.update("watermarkImage", exporter.localPath(selectedFile)) }
    contentItem: ScrollView {
        clip: true
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: 10
            enabled: !exporter.working
            RowLayout {
                Layout.fillWidth: true
                ComboBox { id: profiles; Layout.fillWidth: true; model: exporter.profiles; textRole: "name"; valueRole: "id"; displayText: count ? currentText : "Сохранённые настройки" }
                Button { text: "Загрузить"; enabled: profiles.count > 0; onClicked: exporter.loadProfile(profiles.currentValue) }
                Button { text: "Удалить"; enabled: profiles.count > 0; onClicked: exporter.deleteProfile(profiles.currentValue) }
            }
            RowLayout {
                TextField { id: profileName; placeholderText: "Название настроек"; maximumLength: 80; Layout.fillWidth: true }
                Button { text: "Сохранить"; onClicked: { if (exporter.configure(panel.form)) exporter.saveProfile(profileName.text) } }
            }
            Label { text: "Папка назначения" }
            RowLayout {
                TextField { text: panel.form.destination; Layout.fillWidth: true; onEditingFinished: panel.update("destination", text) }
                Button { text: "Выбрать…"; onClicked: folder.open() }
            }
            Label { text: "Имя файла · {filename}, {sequence}, {date}"; font.pixelSize: 12 }
            TextField { objectName: "exportNaming"; text: panel.form.naming; Layout.fillWidth: true; onEditingFinished: panel.update("naming", text) }
            ComboBox { model: ["Существующие: пропустить", "Существующие: уникальное имя", "Существующие: заменить"]; currentIndex: panel.form.conflict; onActivated: panel.update("conflict", currentIndex); Layout.fillWidth: true }
            Label { visible: panel.form.conflict === 2; text: "Файлы с совпадающими именами будут заменены."; color: "#ffc1bc"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            ComboBox { objectName: "exportFormat"; model: ["JPEG", "PNG", "TIFF · 16 bit", "TIFF · 8 bit", "DNG · обработанный Linear RGB"]; currentIndex: panel.form.format; onActivated: { panel.update("format", currentIndex); if (currentIndex !== 2 && currentIndex !== 3) panel.update("compression", 0) } Layout.fillWidth: true }
            Label { visible: panel.form.format === 4; text: "DNG содержит применённые правки. Это не архив сенсорных данных камеры. Минимум 22 px по каждой стороне."; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 12 }
            RowLayout {
                visible: panel.form.format === 0
                Label { text: "JPEG quality" }
                SpinBox { from: 1; to: 100; value: panel.form.quality; editable: true; onValueModified: panel.update("quality", value) }
            }
            ComboBox { visible: panel.form.format === 2 || panel.form.format === 3; model: ["TIFF · без сжатия", "TIFF · LZW", "TIFF · ZIP"]; currentIndex: panel.form.compression; onActivated: panel.update("compression", currentIndex); Layout.fillWidth: true }
            ComboBox { model: ["sRGB", "Display P3", "Adobe RGB (1998)", "ProPhoto RGB"]; currentIndex: panel.form.colorSpace; onActivated: panel.update("colorSpace", currentIndex); Layout.fillWidth: true }
            Label { text: "Размер после Crop / Transform" }
            RowLayout {
                ComboBox { model: ["Оригинальный", "Ширина, px", "Высота, px", "Длинная сторона, px", "Короткая сторона, px", "Мегапиксели", "Проценты"]; currentIndex: panel.form.resize; onActivated: panel.update("resize", currentIndex); Layout.fillWidth: true }
                TextField { visible: panel.form.resize !== 0; text: panel.form.resizeValue; Layout.preferredWidth: 100; validator: DoubleValidator { bottom: 0.01; top: 20000; locale: "C" } onEditingFinished: panel.update("resizeValue", Number(text)) }
            }
            Label { text: "Резкость для вывода" }
            RowLayout {
                ComboBox { model: ["Выключена", "Screen", "Matte paper", "Glossy paper"]; currentIndex: panel.form.outputMedium; onActivated: panel.update("outputMedium", currentIndex); Layout.fillWidth: true }
                ComboBox { enabled: panel.form.outputMedium !== 0; model: ["Low", "Standard", "High"]; currentIndex: panel.form.outputAmount; onActivated: panel.update("outputAmount", currentIndex) }
            }
            Label { text: "Метаданные" }
            ComboBox { model: ["Не переносить", "Все доступные фото-метаданные", "Только автор / copyright", "Удалить местоположение"]; currentIndex: panel.form.metadata; onActivated: panel.update("metadata", currentIndex); Layout.fillWidth: true }
            Label { text: "MakerNotes, встроенные preview и инструкции обработки камеры не переносятся."; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: "Watermark" }
            ComboBox { model: ["Нет", "Текст", "Изображение"]; currentIndex: panel.form.watermarkKind; onActivated: panel.update("watermarkKind", currentIndex); Layout.fillWidth: true }
            TextField { visible: panel.form.watermarkKind === 1; text: panel.form.watermarkText; placeholderText: "© Автор"; maximumLength: 1024; onEditingFinished: panel.update("watermarkText", text); Layout.fillWidth: true }
            RowLayout {
                visible: panel.form.watermarkKind === 2
                TextField { text: panel.form.watermarkImage; onEditingFinished: panel.update("watermarkImage", text); Layout.fillWidth: true }
                Button { text: "Файл…"; onClicked: watermarkFile.open() }
            }
            GridLayout {
                visible: panel.form.watermarkKind !== 0
                columns: 2; Layout.fillWidth: true
                Label { text: "Непрозрачность, %" }
                SpinBox { from: 0; to: 100; value: panel.form.watermarkOpacity; editable: true; onValueModified: panel.update("watermarkOpacity", value) }
                Label { text: "Размер, % длинной стороны" }
                SpinBox { from: 1; to: 100; value: panel.form.watermarkSize; editable: true; onValueModified: panel.update("watermarkSize", value) }
                Label { text: "Отступ, % короткой стороны" }
                SpinBox { from: 0; to: 25; value: panel.form.watermarkMargin; editable: true; onValueModified: panel.update("watermarkMargin", value) }
                Label { text: "Положение" }
                ComboBox { model: ["Сверху слева", "Сверху по центру", "Сверху справа", "Слева", "По центру", "Справа", "Снизу слева", "Снизу по центру", "Снизу справа"]; currentIndex: panel.form.watermarkAnchor; onActivated: panel.update("watermarkAnchor", currentIndex) }
            }
        }
    }
    footer: ColumnLayout {
        Label { text: exporter.error; textFormat: Text.PlainText; visible: text.length > 0; color: "#ffc1bc"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button { text: "Закрыть"; onClicked: panel.close() }
            Button { text: "Текущее фото"; enabled: editor.hasPhoto && !exporter.working; onClicked: { if (exporter.configure(panel.form)) { exporter.start(false); if (exporter.working) panel.close() } } }
            Button { text: "Выбранные фото"; enabled: library.selectedCount > 0 && !exporter.working; onClicked: { if (exporter.configure(panel.form)) { exporter.start(true); if (exporter.working) panel.close() } } }
        }
    }
}
