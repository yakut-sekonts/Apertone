import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ColumnLayout {
    id: panel
    objectName: "presetPanel"
    property string selectedId: ""
    property var selected: null
    function selection() {
        selected = null
        for (const item of presets.items) {
            if (item.id === selectedId) { selected = item; break }
        }
    }
    function openEditor(create) {
        presets.clearPreview()
        editDialog.presetId = create ? "" : selectedId
        editDialog.copyMode = false
        nameField.text = create ? "" : selected.name
        folderField.text = create ? "" : selected.folder
        editDialog.groups = create ? 127 : selected.groups
        capture.checked = create
        editDialog.open()
    }
    onSelectedIdChanged: selection()
    onVisibleChanged: if (!visible) presets.clearPreview()
    Connections { target: presets; function onItemsChanged() { panel.selection() } }
    spacing: 8
    Label { text: "ПРЕСЕТЫ"; color: "#858c97"; font.pixelSize: 10; font.letterSpacing: 1.5 }
    RowLayout {
        Layout.fillWidth: true; spacing: 4
        Button { objectName: "presetCreate"; implicitWidth: 36; text: "+"; enabled: editor.hasPhoto && !editor.busy; onClicked: panel.openEditor(true); ToolTip.visible: hovered; ToolTip.text: "Сохранить текущие настройки" }
        Button { text: "Импорт"; onClicked: importDialog.open(); Layout.fillWidth: true }
        Button { implicitWidth: 36; text: "⋯"; onClicked: actions.open()
            Menu {
                id: actions
                MenuItem { text: "Изменить / переименовать"; enabled: panel.selected !== null; onTriggered: panel.openEditor(false) }
                MenuItem { text: "Экспорт JSON…"; enabled: panel.selected !== null; onTriggered: { exportDialog.presetId = panel.selectedId; exportDialog.open() } }
                MenuItem { text: "Удалить…"; enabled: panel.selected !== null; onTriggered: deleteDialog.open() }
                MenuSeparator {}
                MenuItem { text: "Обновить список"; onTriggered: presets.refresh() }
            }
        }
    }
    ListView {
        id: list
        objectName: "presetList"
        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 100
        clip: true; model: presets.items
        section.property: "folder"; section.criteria: ViewSection.FullString
        section.delegate: Label {
            required property string section
            objectName: "presetFolderTitle"; textFormat: Text.PlainText
            text: section.length ? section : "Без папки"
            color: "#858c97"; font.pixelSize: 10; height: 26; topPadding: 7
        }
        delegate: ItemDelegate {
            id: presetRow
            required property var modelData
            width: list.width; height: 34
            contentItem: Label {
                objectName: "presetTitle"
                text: presetRow.modelData.name; textFormat: Text.PlainText
                font: presetRow.font; color: presetRow.palette.buttonText
                verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
            }
            text: modelData.name; highlighted: panel.selectedId === modelData.id
            onHoveredChanged: {
                if (hovered && editor.hasPhoto && !editor.busy) presets.preview(modelData.id)
                else if (!hovered) presets.clearPreview()
            }
            onClicked: { panel.selectedId = modelData.id; presets.apply(modelData.id) }
            ToolTip.visible: hovered; ToolTip.delay: 700
            ToolTip.text: "Нажмите, чтобы применить. Undo отменяет все настройки пресета."
        }
        ScrollBar.vertical: ScrollBar {}
        Label {
            anchors.centerIn: parent; width: parent.width
            visible: list.count === 0; text: "Сохраните свои настройки или импортируйте JSON-пресет."
            color: "#858c97"; wrapMode: Text.Wrap; font.pixelSize: 11
        }
    }
    Label { text: presets.error; visible: text.length > 0; color: "#ffc1bc"; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 11 }
    RowLayout {
        Layout.fillWidth: true; spacing: 4
        Button { implicitWidth: 80; Layout.minimumWidth: 0; text: "Copy…"; enabled: editor.hasPhoto && !editor.busy; Layout.fillWidth: true
            onClicked: { presets.clearPreview(); editDialog.copyMode = true; editDialog.groups = 127; editDialog.open() }
        }
        Button { implicitWidth: 80; Layout.minimumWidth: 0; text: "Paste"; enabled: presets.hasCopiedSettings && editor.hasPhoto && !editor.busy; onClicked: presets.pasteSettings(); Layout.fillWidth: true }
    }
    Dialog {
        id: editDialog
        objectName: "presetEditor"
        property string presetId: ""
        property bool copyMode: false
        property int groups: 127
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 430; implicitWidth: 430; modal: true
        title: copyMode ? "Копировать настройки" : presetId.length ? "Изменить пресет" : "Сохранить пресет"
        ColumnLayout {
            width: parent.width; spacing: 10
            TextField { id: nameField; objectName: "presetName"; visible: !editDialog.copyMode; placeholderText: "Название"; maximumLength: 80; Layout.fillWidth: true }
            TextField { id: folderField; objectName: "presetFolder"; visible: !editDialog.copyMode; placeholderText: "Папка (необязательно)"; maximumLength: 80; Layout.fillWidth: true }
            Label { text: "Группы настроек"; color: "#d5d8de" }
            Repeater {
                model: ["Light", "Color", "Curve", "HSL", "Detail", "Crop", "Optics"]
                CheckBox {
                    required property string modelData
                    required property int index
                    objectName: "presetGroup" + index
                    text: modelData; checked: (editDialog.groups & (1 << index)) !== 0
                    onClicked: editDialog.groups = checked ? editDialog.groups | (1 << index) : editDialog.groups & ~(1 << index)
                }
            }
            CheckBox {
                id: capture; visible: !editDialog.copyMode && editDialog.presetId.length > 0
                text: "Заменить настройки текущими правками"
                enabled: editor.hasPhoto && !editor.busy
            }
            Label { visible: !editDialog.copyMode; text: "Переименование и смена папки сохраняют настройки. Для добавления новых групп замените настройки текущими правками."; color: "#858c97"; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 11 }
            Label { text: presets.error; visible: text.length > 0; color: "#ffc1bc"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        }
        footer: DialogButtonBox {
            Button { text: "Отмена"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole; onClicked: editDialog.close() }
            Button {
                objectName: "presetSave"
                text: editDialog.copyMode ? "Копировать" : "Сохранить"
                enabled: editDialog.groups !== 0 && (editDialog.copyMode || nameField.text.trim().length > 0)
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: {
                    if (editDialog.copyMode) { presets.copySettings(editDialog.groups); editDialog.close() }
                    else if (presets.save(editDialog.presetId, nameField.text, folderField.text, editDialog.groups,
                                         !editDialog.presetId.length || (capture.checked && capture.enabled))) editDialog.close()
                }
            }
        }
    }
    Dialog {
        id: deleteDialog
        parent: Overlay.overlay; anchors.centerIn: parent; modal: true
        width: 360; implicitWidth: 360
        title: "Удалить пресет?"
        property string presetId: ""
        onOpened: presetId = panel.selectedId
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label { textFormat: Text.PlainText; text: panel.selected ? panel.selected.name : ""; color: "#d5d8de" }
        onAccepted: presets.remove(presetId)
    }
    FileDialog { id: importDialog; title: "Импорт пресета Luma"; nameFilters: ["Luma preset (*.json)"]; onAccepted: presets.importPreset(selectedFile) }
    FileDialog {
        id: exportDialog; property string presetId: ""
        title: "Экспорт пресета Luma"; fileMode: FileDialog.SaveFile
        nameFilters: ["Luma preset (*.json)"]; defaultSuffix: "json"
        onAccepted: presets.exportPreset(presetId, selectedFile)
    }
}
