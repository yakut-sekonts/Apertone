import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Rectangle {
    id: pane
    color: "#111318"
    property var sources: []
    property url destination: ""
    property string currentCollection: ""
    property string editCollectionID: ""
    function applyFilter() { library.filter(search.text, ratingFilter.currentIndex, flagFilter.currentIndex - 2, ["any","","red","yellow","green","blue","purple"][labelFilter.currentIndex], currentCollection) }
    function allowShortcut() { return library.selectedCount > 0 && !library.working && !(Window.window.activeFocusItem && typeof Window.window.activeFocusItem.cursorPosition === "number") }
    Shortcut { sequence: "P"; enabled: pane.visible && pane.allowShortcut(); onActivated: library.flag(1) }
    Shortcut { sequence: "X"; enabled: pane.visible && pane.allowShortcut(); onActivated: library.flag(-1) }
    Shortcut { sequence: "U"; enabled: pane.visible && pane.allowShortcut(); onActivated: library.flag(0) }
    Repeater {
        model: 6
        Item { required property int index; Shortcut { sequence: index.toString(); enabled: pane.visible && pane.allowShortcut(); onActivated: library.rate(index) } }
    }
    Shortcut { sequences: [StandardKey.SelectAll]; enabled: pane.visible && !(search.activeFocus); onActivated: library.selectAll() }
    FileDialog { id: files; title: "Импорт RAW"; fileMode: FileDialog.OpenFiles; nameFilters: ["RAW (*.NEF *.nef *.DNG *.dng *.CR2 *.cr2 *.CR3 *.cr3 *.ARW *.arw *.RW2 *.rw2 *.ORF *.orf)", "Все файлы (*)"]; onAccepted: pane.sources = selectedFiles }
    FolderDialog { id: folder; title: "Папка с RAW"; onAccepted: pane.sources = [selectedFolder] }
    FolderDialog { id: copyFolder; title: "Куда скопировать оригиналы"; onAccepted: pane.destination = selectedFolder }
    Dialog {
        id: importDialog; title: "Импорт фотографий"; modal: true; anchors.centerIn: parent; width: 550
        standardButtons: Dialog.Cancel
        ColumnLayout {
            width: parent.width; spacing: 12
            RowLayout { Button { text: "Выбрать файлы"; onClicked: files.open() } Button { text: "Выбрать папку"; onClicked: folder.open() } }
            Label { text: pane.sources.length ? "Источников: " + pane.sources.length + " · " + pane.sources[0] : "Файлы или папка ещё не выбраны"; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; font.pixelSize: 11 }
            ComboBox { id: importMode; model: ["Add · оставить оригиналы на месте", "Copy · создать копии в выбранной папке"]; Layout.fillWidth: true }
            Button { visible: importMode.currentIndex === 1; text: "Выбрать папку назначения"; onClicked: copyFolder.open() }
            Label { visible: importMode.currentIndex === 1; text: pane.destination.toString() || "Папка не выбрана"; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; font.pixelSize: 11 }
            CheckBox { id: recursive; text: "Включить вложенные папки"; checked: true }
            CheckBox { id: duplicates; text: "Пропускать дубликаты по SHA-256 и размеру"; checked: true }
            Label { text: "Copy создаёт папку Luma с отдельным каталогом для каждого файла. Уже добавленные фотографии сохраняются при отмене."; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 11; color: "#8893a3" }
            Button { text: "Импортировать"; enabled: pane.sources.length > 0 && (importMode.currentIndex === 0 || pane.destination.toString().length > 0) && !library.working; onClicked: { library.importFiles(pane.sources, importMode.currentIndex === 1, pane.destination, recursive.checked, duplicates.checked); importDialog.close() } }
        }
    }
    Dialog {
        id: collectionDialog; title: pane.editCollectionID ? "Переименовать коллекцию" : "Новая коллекция"; modal: true; anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        TextField { id: collectionName; width: 300; placeholderText: "Название коллекции"; maximumLength: 128; selectByMouse: true }
        onAccepted: { if (pane.editCollectionID) library.renameCollection(pane.editCollectionID, collectionName.text); else library.createCollection(collectionName.text) }
    }
    Dialog {
        id: deleteDialog; title: "Удалить коллекцию?"; modal: true; anchors.centerIn: parent; standardButtons: Dialog.Ok | Dialog.Cancel
        Label { text: "Фотографии и правки останутся в каталоге." }
        onAccepted: { library.deleteCollection(pane.currentCollection); pane.currentCollection = ""; pane.applyFilter() }
    }
    Dialog {
        id: reportDialog; title: "Ошибки последнего импорта"; modal: true; anchors.centerIn: parent
        width: Math.min(pane.width - 40, 700); height: Math.min(pane.height - 40, 450)
        standardButtons: Dialog.Close
        ScrollView { anchors.fill: parent; clip: true; TextArea { text: library.importReport; readOnly: true; wrapMode: Text.WrapAnywhere; textFormat: TextEdit.PlainText; selectByMouse: true } }
    }
    RowLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            color: "#191c22"; Layout.preferredWidth: 205; Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 16; spacing: 12
                Label { text: "БИБЛИОТЕКА"; font.pixelSize: 10; color: "#8290a1"; font.letterSpacing: 1.5 }
                Button { text: "+  Импорт"; Layout.fillWidth: true; enabled: !library.working; onClicked: importDialog.open() }
                Button { text: "Все фотографии"; Layout.fillWidth: true; highlighted: !pane.currentCollection; onClicked: { pane.currentCollection = ""; pane.applyFilter() } }
                Label { text: "КОЛЛЕКЦИИ"; font.pixelSize: 10; color: "#8290a1"; Layout.topMargin: 16 }
                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
                    Column {
                        width: parent.width; spacing: 4
                        Repeater {
                            model: library.collections
                            Button { required property var modelData; width: parent.width; text: modelData.name + " · " + modelData.count; highlighted: pane.currentCollection === modelData.id; onClicked: { pane.currentCollection = modelData.id; pane.applyFilter() } }
                        }
                    }
                }
                Button { text: "+ Коллекция"; Layout.fillWidth: true; enabled: !library.working; onClicked: { pane.editCollectionID = ""; collectionName.text = ""; collectionDialog.open() } }
                Button { text: "Переименовать"; visible: !!pane.currentCollection; enabled: !library.working; Layout.fillWidth: true; onClicked: { pane.editCollectionID = pane.currentCollection; const item = library.collections.find(c => c.id === pane.currentCollection); collectionName.text = item ? item.name : ""; collectionDialog.open() } }
                Button { text: "Удалить коллекцию"; visible: !!pane.currentCollection; enabled: !library.working; Layout.fillWidth: true; onClicked: deleteDialog.open() }
                Label { text: "Двойной клик → Develop\n⌘ / Shift → выделение\n0–5 · P / X / U"; color: "#8290a1"; font.pixelSize: 10; Layout.topMargin: 8 }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.margins: 18; spacing: 12
            RowLayout {
                Layout.fillWidth: true
                TextField { id: search; placeholderTextColor: "#858c97"; placeholderText: "Поиск: имя, путь, камера, объектив"; selectByMouse: true; maximumLength: 1024; Layout.fillWidth: true; onTextChanged: pane.applyFilter() }
                ComboBox { id: ratingFilter; model: ["Все ★", "≥ 1 ★", "≥ 2 ★", "≥ 3 ★", "≥ 4 ★", "5 ★"]; onActivated: pane.applyFilter() }
                ComboBox { id: flagFilter; model: ["Все флаги", "Reject", "Без флага", "Pick"]; onActivated: pane.applyFilter() }
                ComboBox { id: labelFilter; model: ["Все метки", "Без метки", "Красная", "Жёлтая", "Зелёная", "Синяя", "Фиолетовая"]; onActivated: pane.applyFilter() }
            }
            RowLayout {
                visible: library.importing; Layout.fillWidth: true
                ProgressBar { value: library.progress; indeterminate: library.progress === 0; Layout.fillWidth: true }
                Button { text: "Отменить импорт"; onClicked: library.cancelImport() }
            }
            Label { visible: library.error.length > 0; text: library.error; color: "#ffc1bc"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                Button { visible: library.importReport.length > 0; text: "Ошибки импорта"; onClicked: reportDialog.open() }
                Label { text: library.loading ? "Загрузка…" : library.count + " фотографий · выбрано " + library.selectedCount; color: "#98a3b3"; Layout.fillWidth: true }
                Button { text: "Выбрать всё"; enabled: library.count > 0; onClicked: library.selectAll() }
                Button { text: "Снять выделение"; enabled: library.selectedCount > 0; onClicked: library.clearSelection() }
            }
            Item {
                Layout.fillWidth: true; Layout.fillHeight: true
                GridView {
                    id: grid; anchors.fill: parent; clip: true; model: library
                    cellWidth: width / Math.max(1,Math.floor(width / 220)); cellHeight: 210
                    cacheBuffer: 420
                    delegate: PhotoCard { width: grid.cellWidth - 12; height: grid.cellHeight - 12 }
                    ScrollBar.vertical: ScrollBar {}
                }
                Label { anchors.centerIn: parent; visible: !library.loading && !library.count; text: "Добавьте RAW через Импорт\nили измените фильтры"; horizontalAlignment: Text.AlignHCenter; color: "#8290a1"; font.pixelSize: 18 }
            }
            RowLayout {
                enabled: library.selectedCount > 0 && !library.working; Layout.fillWidth: true; spacing: 6
                Label { text: "Оценка"; font.pixelSize: 11 }
                Repeater { model: 6; Button { required property int index; text: index ? index + "★" : "0"; implicitWidth: 38; onClicked: library.rate(index) } }
                ComboBox { model: ["Флаг…", "Pick", "Reject", "Без флага"]; onActivated: { if (currentIndex > 0) library.flag([0,1,-1,0][currentIndex]); currentIndex = 0 } }
                ComboBox { model: ["Метка…", "Без метки", "Красная", "Жёлтая", "Зелёная", "Синяя", "Фиолетовая"]; onActivated: { if (currentIndex > 0) library.label(["","","red","yellow","green","blue","purple"][currentIndex]); currentIndex = 0 } }
                Item { Layout.fillWidth: true }
                ComboBox { id: targetCollection; model: library.collections; textRole: "name"; Layout.maximumWidth: 145 }
                Button { text: "+"; enabled: library.collections.length > 0; onClicked: library.collectionMembership(library.collections[targetCollection.currentIndex].id,true); Accessible.name: "Добавить в коллекцию" }
                Button { text: "−"; enabled: !!pane.currentCollection; onClicked: library.collectionMembership(pane.currentCollection,false); Accessible.name: "Убрать из коллекции" }
            }
            Label { text: library.status; font.pixelSize: 10; color: "#8290a1"; elide: Text.ElideRight; Layout.fillWidth: true }
        }
    }
}
