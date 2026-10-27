import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GridLayout {
    id: row
    columns: width < 900 ? 3 : 6
    columnSpacing: 6
    rowSpacing: 6
    objectName: "batchSettings"
    Layout.minimumWidth: 0
    Layout.fillWidth: true
    property bool ready: library.selectedCount > 0 && !library.working && !library.selecting && !editor.busy && !editor.exporting
    property var availablePresets: presets.builtinItems.concat(presets.items)
    ApertoneComboBox {
        Layout.minimumWidth: 0;
        id: presetChoice; objectName: "batchPresetChoice"
        model: row.availablePresets; textRole: "name"; Layout.preferredWidth: 190; Layout.maximumWidth: 230
        enabled: row.ready && row.availablePresets.length > 0
        delegate: ItemDelegate {
            required property var modelData
            width: presetChoice.width
            contentItem: Label { text: modelData.folder ? modelData.folder + " / " + modelData.name : modelData.name; textFormat: Text.PlainText; elide: Text.ElideRight }
        }
        contentItem: Label { text: presetChoice.currentText; textFormat: Text.PlainText; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; leftPadding: 8 }
    }
    ApertoneButton { objectName: "batchApplyPreset"; Layout.minimumWidth: 0; Layout.maximumWidth: 110; ToolTip.visible: hovered; ToolTip.text: text; text: qsTranslate("Apertone", "Apply"); enabled: row.ready && presetChoice.currentIndex >= 0; onClicked: batch.applyPreset(row.availablePresets[presetChoice.currentIndex].id) }
    ApertoneButton { objectName: "batchPaste"; Layout.minimumWidth: 0; Layout.maximumWidth: 110; ToolTip.visible: hovered; ToolTip.text: text; text: qsTranslate("Apertone", "Paste"); enabled: row.ready && presets.hasCopiedSettings; onClicked: batch.paste() }
    ApertoneButton { objectName: "batchSync"; Layout.minimumWidth: 0; Layout.maximumWidth: 200; ToolTip.visible: hovered; ToolTip.text: text; text: qsTranslate("Apertone", "Sync Settings…"); enabled: row.ready && editor.hasPhoto; onClicked: syncDialog.open() }
    ApertoneButton { objectName: "batchUndo"; Layout.minimumWidth: 0; Layout.maximumWidth: 160; ToolTip.visible: hovered; ToolTip.text: text; text: qsTranslate("Apertone", "Undo batch"); enabled: batch.canUndo && !library.working && !editor.busy && !editor.exporting; onClicked: batch.undo() }
    ApertoneButton { objectName: "batchRedo"; Layout.minimumWidth: 0; Layout.maximumWidth: 160; ToolTip.visible: hovered; ToolTip.text: text; text: qsTranslate("Apertone", "Redo batch"); enabled: batch.canRedo && !library.working && !editor.busy && !editor.exporting; onClicked: batch.redo() }
    Dialog {
        id: syncDialog; objectName: "batchSyncDialog"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: 430; implicitWidth: 430; modal: true
        title: qsTranslate("Apertone", "Sync Settings")
        property int groups: 4095
        ColumnLayout {
            width: parent.width
            Label { text: qsTranslate("Apertone", "Source: ") + editor.filename; textFormat: Text.PlainText; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: qsTranslate("Apertone", "Selected photos: ") + library.selectedCount + qsTranslate("Apertone", ". Only the selected groups will change."); wrapMode: Text.Wrap; Layout.fillWidth: true }
            Repeater {
                model: [qsTranslate("Apertone", "Light"), qsTranslate("Apertone", "Color"), qsTranslate("Apertone", "Curve"), "HSL", qsTranslate("Apertone", "Detail"), qsTranslate("Apertone", "Crop"), qsTranslate("Apertone", "Optics"), qsTranslate("Apertone", "Geometry"),qsTranslate("Apertone","Presence"),qsTranslate("Apertone","Point Color"),qsTranslate("Apertone","Color Grading"),qsTranslate("Apertone","Effects")]
                CheckBox {
                    required property string modelData
                    required property int index
                    text: modelData; checked: (syncDialog.groups & (1 << index)) !== 0
                    onClicked: syncDialog.groups = checked ? syncDialog.groups | (1 << index) : syncDialog.groups & ~(1 << index)
                }
            }
        }
        footer: DialogButtonBox {
            ApertoneButton { text: qsTranslate("Apertone", "Cancel"); onClicked: syncDialog.close(); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            ApertoneButton { objectName: "batchSyncConfirm"; text: qsTranslate("Apertone", "Apply to selection"); enabled: syncDialog.groups !== 0 && row.ready; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole; onClicked: { batch.sync(syncDialog.groups); syncDialog.close() } }
        }
    }
}
