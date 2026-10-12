import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 1320
    height: 840
    minimumWidth: 1000
    minimumHeight: 640
    title: "Luma" + (editor.hasPhoto ? " — " + editor.filename : "")
    color: "#111318"
    palette.window: "#111318"
    palette.base: "#1b1e24"
    palette.text: "#e3e5e9"
    palette.buttonText: "#e3e5e9"
    palette.highlight: "#dfae77"
    palette.button: "#252a32"
    font.family: "Helvetica Neue"
    onClosing: function(close) { close.accepted = editor.requestClose() }
    Connections { target: editor; function onCloseReady() { root.close() } }
    Shortcut { sequences: [StandardKey.Open]; onActivated: fileDialog.open() }
    Shortcut { sequences: [StandardKey.Undo]; enabled: editor.canUndo && !editor.busy; onActivated: editor.undo() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: editor.canRedo && !editor.busy; onActivated: editor.redo() }
    Shortcut { sequences: [StandardKey.Quit]; onActivated: root.close() }
    Shortcut { sequence: "\\"; onActivated: editor.original = !editor.original }
    Shortcut { sequence: "Ctrl+Shift+F"; onActivated: root.visibility = root.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen }
    FileDialog {
        id: fileDialog
        title: "Открыть RAW"
        nameFilters: ["Bayer RAW (*.NEF *.nef *.DNG *.dng *.CR2 *.cr2 *.CR3 *.cr3 *.ARW *.arw *.RW2 *.rw2 *.ORF *.orf)", "Все файлы (*)"]
        onAccepted: editor.openFile(selectedFile)
    }
    Dialog {
        id: exportSettings
        title: "Экспорт"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: exportDialog.open()
        ColumnLayout {
            spacing: 12
            Label { text: "Исходное разрешение · sRGB · без метаданных"; font.pixelSize: 12 }
            ComboBox { id: exportFormat; model: ["JPEG", "PNG", "TIFF · 16 bit"]; Layout.fillWidth: true }
            Label { text: "JPEG quality: " + exportQuality.value }
            Slider { id: exportQuality; from: 1; to: 100; stepSize: 1; value: 92; enabled: exportFormat.currentIndex === 0; Layout.fillWidth: true }
            CheckBox { id: overwriteExport; text: "Разрешить замену существующего файла"; checked: false }
        }
    }
    FileDialog {
        id: exportDialog
        title: "Экспорт фотографии"
        fileMode: FileDialog.SaveFile
        nameFilters: exportFormat.currentIndex === 0 ? ["JPEG (*.jpg *.jpeg)"] : exportFormat.currentIndex === 1 ? ["PNG (*.png)"] : ["TIFF (*.tif *.tiff)"]
        defaultSuffix: exportFormat.currentIndex === 0 ? "jpg" : exportFormat.currentIndex === 1 ? "png" : "tif"
        onAccepted: editor.exportPhoto(selectedFile, exportFormat.currentIndex, Math.round(exportQuality.value), overwriteExport.checked)
    }
    DropArea {
        anchors.fill: parent
        onDropped: function(drop) {
            if (drop.hasUrls && drop.urls.length === 1) { editor.openFile(drop.urls[0]); drop.acceptProposedAction() }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 64; color: "#191c22"
            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 18
                Label { text: "◐"; color: "#dfae77"; font.pixelSize: 28 }
                Label { text: "luma"; color: "#f0f1f3"; font.pixelSize: 24; font.weight: Font.DemiBold; font.letterSpacing: 2 }
                Label { text: "RAW WORKSPACE"; color: "#747b88"; font.pixelSize: 10; font.letterSpacing: 2 }
                Item { Layout.fillWidth: true }
                Label { text: editor.filename; color: "#989fab"; font.pixelSize: 12; elide: Text.ElideMiddle; Layout.maximumWidth: 350 }
                Button { text: "↶"; enabled: editor.canUndo && !editor.busy; onClicked: editor.undo(); Accessible.name: "Undo" }
                Button { text: "↷"; enabled: editor.canRedo && !editor.busy; onClicked: editor.redo(); Accessible.name: "Redo" }
                Button { text: "Экспорт"; enabled: editor.hasPhoto && !editor.busy && !editor.exporting; onClicked: exportSettings.open() }
                Button { text: "+  Открыть RAW"; enabled: !editor.busy; onClicked: fileDialog.open() }
            }
        }
        Rectangle {
            visible: editor.error.length > 0
            Layout.fillWidth: true; Layout.preferredHeight: visible ? errorLayout.implicitHeight + 24 : 0; color: "#42282a"
            RowLayout {
                id: errorLayout; anchors.fill: parent; anchors.margins: 12
                Label { text: editor.error; color: "#ffc1bc"; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 12 }
                Button { visible: editor.saving && !editor.busy; text: "Повторить запись"; onClicked: editor.retrySave() }
            }
        }
        RowLayout {
            visible: editor.exporting
            Layout.fillWidth: true; Layout.margins: visible ? 10 : 0
            Label { text: "Экспорт"; color: "#dfae77" }
            ProgressBar { value: editor.exportProgress; Layout.fillWidth: true }
            Button { text: "Отменить"; onClicked: editor.cancelExport() }
        }
        SplitView {
            id: panels
            Layout.fillWidth: true; Layout.fillHeight: true
            orientation: Qt.Horizontal
            handle: Rectangle { implicitWidth: 1; color: "#30343b" }
            Rectangle {
                SplitView.preferredWidth: 214; SplitView.minimumWidth: 170; SplitView.maximumWidth: 350; color: "#191c22"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 18
                    Label { text: "РАБОЧЕЕ ПРОСТРАНСТВО"; color: "#777f8b"; font.pixelSize: 9; font.letterSpacing: 1 }
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: 42; radius: 6; color: "#302a25"
                        Label { anchors.centerIn: parent; text: "Обработка RAW"; color: "#dfae77"; font.pixelSize: 12 }
                    }
                    Label { text: "ИСХОДНОЕ ФОТО"; color: "#777f8b"; font.pixelSize: 9; font.letterSpacing: 1; Layout.topMargin: 10 }
                    Label { text: editor.hasPhoto ? editor.filename : "Фото не выбрано"; color: "#d5d8de"; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    Label { text: editor.dimensions; color: "#777f8b"; font.pixelSize: 11 }
                    Label { text: "Оригинал без изменений"; color: "#858c97"; font.pixelSize: 10; visible: editor.hasPhoto }
                    Item { Layout.fillHeight: true }
                    Label { text: "КАТАЛОГ"; color: "#777f8b"; font.pixelSize: 9; font.letterSpacing: 1 }
                    Label { text: editor.catalogPath; color: "#777f8b"; font.pixelSize: 10; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                    Label { text: "LUMA / DEVELOP CORE"; color: "#a28a70"; font.pixelSize: 9; font.letterSpacing: 2 }
                }
            }
            Rectangle {
                SplitView.fillWidth: true; SplitView.minimumWidth: 360; color: "#111318"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 24; spacing: 18
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: editor.original ? "ОРИГИНАЛЬНЫЙ RAW" : "ПРЕДПРОСМОТР"; color: "#777f8b"; font.pixelSize: 9; font.letterSpacing: 2 }
                        Item { Layout.fillWidth: true }
                        Label { text: "FIT"; color: "#777f8b"; font.pixelSize: 10 }
                    }
                    Item {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        Image { anchors.fill: parent; source: editor.previewUrl; fillMode: Image.PreserveAspectFit; cache: false; visible: editor.hasPhoto && !editor.busy }
                        ColumnLayout {
                            anchors.centerIn: parent; spacing: 18; visible: !editor.hasPhoto && !editor.busy
                            Label { text: "◉"; font.pixelSize: 72; color: "#dfae77"; Layout.alignment: Qt.AlignHCenter }
                            Label { text: "Всё начинается с RAW."; color: "#ece8e2"; font.pixelSize: 25; font.weight: Font.Light; Layout.alignment: Qt.AlignHCenter }
                            Label { text: "Откройте кадр. Найдите свой свет."; color: "#858c98"; font.pixelSize: 12; Layout.alignment: Qt.AlignHCenter }
                            Button { text: "Открыть фотографию"; Layout.alignment: Qt.AlignHCenter; onClicked: fileDialog.open() }
                            Label { text: "Nikon NEF · Bayer DNG"; color: "#777f8b"; font.pixelSize: 10; Layout.alignment: Qt.AlignHCenter }
                        }
                        ColumnLayout {
                            anchors.centerIn: parent; visible: editor.busy; spacing: 16; width: Math.min(parent.width - 60, 300)
                            BusyIndicator { running: editor.busy; Layout.alignment: Qt.AlignHCenter }
                            Label { text: "Декодирование данных сенсора"; color: "#d4d8df"; font.pixelSize: 12; Layout.alignment: Qt.AlignHCenter }
                            ProgressBar { value: editor.progress; Layout.fillWidth: true }
                            Button { text: "Отменить"; onClicked: editor.cancelLoad(); Layout.alignment: Qt.AlignHCenter }
                        }
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter; spacing: 8
                        Button { text: "Обработка"; checkable: true; checked: !editor.original; enabled: editor.hasPhoto && !editor.busy; onClicked: editor.original = false }
                        Button { text: "Оригинал"; checkable: true; checked: editor.original; enabled: editor.hasPhoto && !editor.busy; onClicked: editor.original = true }
                    }
                }
            }
            Rectangle {
                SplitView.preferredWidth: 290; SplitView.minimumWidth: 250; SplitView.maximumWidth: 400; color: "#191c22"
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 18
                    clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width
                        spacing: 14
                        Label { text: "Коррекция"; font.pixelSize: 17; font.weight: Font.DemiBold; color: "#e5e7ec" }
                        RowLayout {
                            CheckBox { checked: editor.correctionStates.whiteBalance; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setCorrectionEnabled("whiteBalance",checked); padding: 0 }
                            Label { text: "БАЛАНС БЕЛОГО"; color: "#858c97"; font.pixelSize: 9; font.letterSpacing: 1 }
                        }
                        ComboBox {
                            Layout.fillWidth: true
                            model: ["As Shot","Daylight","Cloudy","Shade","Tungsten","Fluorescent","Custom"]
                            currentIndex: editor.customWhiteBalance ? 6 : 0
                            enabled: editor.hasPhoto && !editor.busy && editor.correctionStates.whiteBalance
                            onActivated: editor.setWhiteBalancePreset(currentText)
                        }
                        DevelopSlider { parameter: "temperature"; label: "Temperature"; minimum: 2000; maximum: 25000; step: 50; defaultValue: 6500; suffix: " K"; correction: "whiteBalance"; allowDisable: false; Layout.fillWidth: true }
                        DevelopSlider { parameter: "tint"; label: "Tint"; minimum: -150; maximum: 150; correction: "whiteBalance"; allowDisable: false; Layout.fillWidth: true }
                        Label { visible: !editor.customWhiteBalance; text: "As Shot: WB из RAW. Kelvin доступен в Custom."; font.pixelSize: 10; color: "#858c97"; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#30343b" }
                        Label { text: "СВЕТ"; color: "#858c97"; font.pixelSize: 9; font.letterSpacing: 2 }
                        DevelopSlider { parameter: "exposure"; label: "Exposure"; minimum: -5; maximum: 5; step: 0.01; suffix: " EV"; Layout.fillWidth: true }
                        DevelopSlider { parameter: "contrast"; label: "Contrast"; Layout.fillWidth: true }
                        DevelopSlider { parameter: "highlights"; label: "Highlights"; Layout.fillWidth: true }
                        DevelopSlider { parameter: "shadows"; label: "Shadows"; Layout.fillWidth: true }
                        DevelopSlider { parameter: "whites"; label: "Whites"; Layout.fillWidth: true }
                        DevelopSlider { parameter: "blacks"; label: "Blacks"; Layout.fillWidth: true }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#30343b" }
                        Label { text: "ЦВЕТ"; color: "#858c97"; font.pixelSize: 9; font.letterSpacing: 2 }
                        DevelopSlider { parameter: "vibrance"; label: "Vibrance"; Layout.fillWidth: true }
                        DevelopSlider { parameter: "saturation"; label: "Saturation"; Layout.fillWidth: true }
                        Button { text: "Сбросить все правки"; enabled: editor.hasPhoto && !editor.busy; onClicked: editor.resetEdits(); Layout.fillWidth: true }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#30343b" }
                        Label { text: "ДАННЫЕ СЪЁМКИ"; color: "#858c97"; font.pixelSize: 9; font.letterSpacing: 1.5 }
                        MetadataRow { label: "Камера"; value: editor.camera; Layout.fillWidth: true }
                        MetadataRow { label: "Объектив"; value: editor.lens; Layout.fillWidth: true }
                        MetadataRow { label: "ISO"; value: editor.iso; Layout.fillWidth: true }
                        MetadataRow { label: "Выдержка"; value: editor.shutter; Layout.fillWidth: true }
                        MetadataRow { label: "Диафрагма"; value: editor.aperture; Layout.fillWidth: true }
                        MetadataRow { label: "Фокусное"; value: editor.focalLength; Layout.fillWidth: true }
                        Label { text: editor.saving ? "Сохраняем изменения…" : "Изменения сохраняются автоматически"; color: "#858c97"; font.pixelSize: 10; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 34; color: "#191c22"
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 20
                Rectangle { width: 5; height: 5; radius: 3; color: editor.busy || editor.saving ? "#dfae77" : "#7bad87" }
                Label { text: editor.status; font.pixelSize: 10; color: "#858c97"; Layout.fillWidth: true; elide: Text.ElideRight }
                Label { text: "Правки не изменяют исходный RAW"; color: "#777f8b"; font.pixelSize: 10 }
            }
        }
    }
}
