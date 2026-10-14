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
        editDialog.groups = create ? 255 : selected.groups
        capture.checked = create
        editDialog.open()
    }
    onSelectedIdChanged: selection()
    onVisibleChanged: if (!visible) presets.clearPreview()
    Connections { target: presets; function onItemsChanged() { panel.selection() } }
    spacing: 8
    TabBar {
        id: presetTabs
        objectName: "presetTabs"
        Layout.fillWidth: true
        onCurrentIndexChanged: presets.clearPreview()
        TabButton { text: qsTranslate("Luma", "Filters"); implicitWidth: 80; font.pixelSize: 11 }
        TabButton { text: qsTranslate("Luma", "Presets"); implicitWidth: 80; font.pixelSize: 11 }
    }
    ListView {
        id: builtinList
        objectName: "builtinFilterList"
        visible: presetTabs.currentIndex === 0
        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 100
        clip: true; model: presets.builtinItems
        delegate: ItemDelegate {
            id: filterRow
            required property var modelData
            objectName: "filter_" + modelData.id.substring(8)
            width: builtinList.width; height: 30; text: modelData.name; hoverEnabled: true
            enabled: editor.hasPhoto && !editor.busy
            onHoveredChanged: {
                if (hovered && enabled) presets.preview(modelData.id)
                else if (!hovered) presets.clearPreview()
            }
            onClicked: presets.apply(modelData.id)
            ToolTip.visible: hovered; ToolTip.delay: 700
            ToolTip.text: qsTranslate("Luma", "Curves and HSL. Exposure, white balance, Detail and geometry are preserved. One Undo step.")
        }
        ScrollBar.vertical: ScrollBar {}
    }
    RowLayout {
        visible: presetTabs.currentIndex === 1
        Layout.fillWidth: true; spacing: 4
        Button { objectName: "presetCreate"; implicitWidth: 36; text: "+"; enabled: editor.hasPhoto && !editor.busy; onClicked: panel.openEditor(true); ToolTip.visible: hovered; ToolTip.text: qsTranslate("Luma", "Save current settings") }
        Button { text: qsTranslate("Luma", "Import"); onClicked: importDialog.open(); Layout.fillWidth: true }
        Button { implicitWidth: 36; text: "⋯"; onClicked: actions.open()
            Menu {
                id: actions
                MenuItem { text: qsTranslate("Luma", "Edit / rename"); enabled: panel.selected !== null; onTriggered: panel.openEditor(false) }
                MenuItem { text: qsTranslate("Luma", "Export JSON…"); enabled: panel.selected !== null; onTriggered: { exportDialog.presetId = panel.selectedId; exportDialog.open() } }
                MenuItem { text: qsTranslate("Luma", "Delete…"); enabled: panel.selected !== null; onTriggered: deleteDialog.open() }
                MenuSeparator {}
                MenuItem { text: qsTranslate("Luma", "Refresh"); onTriggered: presets.refresh() }
            }
        }
    }
    ListView {
        id: list
        objectName: "presetList"
        visible: presetTabs.currentIndex === 1
        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 100
        clip: true; model: presets.items
        section.property: "folder"; section.criteria: ViewSection.FullString
        section.delegate: Label {
            required property string section
            objectName: "presetFolderTitle"; textFormat: Text.PlainText
            text: section.length ? section : qsTranslate("Luma", "No folder")
            color: "#858c97"; font.pixelSize: 10; height: 26; topPadding: 7
        }
        delegate: ItemDelegate {
            id: presetRow
            required property var modelData
            width: list.width; height: 34; hoverEnabled: true
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
            ToolTip.text: qsTranslate("Luma", "Click to apply. Undo restores all preset settings.")
        }
        ScrollBar.vertical: ScrollBar {}
        Label {
            anchors.centerIn: parent; width: parent.width
            visible: list.count === 0; text: qsTranslate("Luma", "Save your settings or import a JSON preset.")
            color: "#858c97"; wrapMode: Text.Wrap; font.pixelSize: 11
        }
    }
    Label { text: presets.error; visible: text.length > 0; color: "#ffc1bc"; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 11 }
    RowLayout {
        Layout.fillWidth: true; spacing: 4
        Button { implicitWidth: 80; Layout.minimumWidth: 0; text: qsTranslate("Luma", "Copy…"); enabled: editor.hasPhoto && !editor.busy; Layout.fillWidth: true
            onClicked: { presets.clearPreview(); editDialog.copyMode = true; editDialog.groups = 255; editDialog.open() }
        }
        Button { implicitWidth: 80; Layout.minimumWidth: 0; text: qsTranslate("Luma", "Paste"); enabled: presets.hasCopiedSettings && editor.hasPhoto && !editor.busy; onClicked: presets.pasteSettings(); Layout.fillWidth: true }
    }
    Dialog {
        id: editDialog
        objectName: "presetEditor"
        property string presetId: ""
        property bool copyMode: false
        property int groups: 255
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 430; implicitWidth: 430; modal: true
        title: copyMode ? qsTranslate("Luma", "Copy settings") : presetId.length ? qsTranslate("Luma", "Edit preset") : qsTranslate("Luma", "Save preset")
        ColumnLayout {
            width: parent.width; spacing: 10
            TextField { id: nameField; objectName: "presetName"; visible: !editDialog.copyMode; placeholderText: qsTranslate("Luma", "Name"); maximumLength: 80; Layout.fillWidth: true }
            TextField { id: folderField; objectName: "presetFolder"; visible: !editDialog.copyMode; placeholderText: qsTranslate("Luma", "Folder (optional)"); maximumLength: 80; Layout.fillWidth: true }
            Label { text: qsTranslate("Luma", "Settings groups"); color: "#d5d8de" }
            Repeater {
                model: [qsTranslate("Luma", "Light"), qsTranslate("Luma", "Color"), qsTranslate("Luma", "Curve"), "HSL", qsTranslate("Luma", "Detail"), qsTranslate("Luma", "Crop"), qsTranslate("Luma", "Optics"), qsTranslate("Luma", "Geometry")]
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
                text: qsTranslate("Luma", "Replace settings with current edits")
                enabled: editor.hasPhoto && !editor.busy
            }
            Label { visible: !editDialog.copyMode; text: qsTranslate("Luma", "Renaming or moving preserves settings. Replace with current edits to add new groups."); color: "#858c97"; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 11 }
            Label { text: presets.error; visible: text.length > 0; color: "#ffc1bc"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        }
        footer: DialogButtonBox {
            Button { text: qsTranslate("Luma", "Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole; onClicked: editDialog.close() }
            Button {
                objectName: "presetSave"
                text: editDialog.copyMode ? qsTranslate("Luma", "Copy") : qsTranslate("Luma", "Save")
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
        title: qsTranslate("Luma", "Delete preset?")
        property string presetId: ""
        onOpened: presetId = panel.selectedId
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label { textFormat: Text.PlainText; text: panel.selected ? panel.selected.name : ""; color: "#d5d8de" }
        onAccepted: presets.remove(presetId)
    }
    FileDialog { id: importDialog; title: qsTranslate("Luma", "Import Luma preset"); nameFilters: ["Luma preset (*.json)"]; onAccepted: presets.importPreset(selectedFile) }
    FileDialog {
        id: exportDialog; property string presetId: ""
        title: qsTranslate("Luma", "Export Luma preset"); fileMode: FileDialog.SaveFile
        nameFilters: ["Luma preset (*.json)"]; defaultSuffix: "json"
        onAccepted: presets.exportPreset(presetId, selectedFile)
    }
}
