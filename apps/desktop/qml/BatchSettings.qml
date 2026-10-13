import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: row
    objectName: "batchSettings"
    Layout.fillWidth: true
    property bool ready: library.selectedCount > 0 && !library.working && !library.selecting && !editor.busy && !editor.exporting
    ComboBox {
        id: presetChoice; objectName: "batchPresetChoice"
        model: presets.items; textRole: "name"; Layout.preferredWidth: 190; Layout.maximumWidth: 230
        enabled: row.ready && presets.items.length > 0
        delegate: ItemDelegate {
            required property var modelData
            width: presetChoice.width
            contentItem: Label { text: modelData.folder ? modelData.folder + " / " + modelData.name : modelData.name; textFormat: Text.PlainText; elide: Text.ElideRight }
        }
        contentItem: Label { text: presetChoice.currentText; textFormat: Text.PlainText; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; leftPadding: 8 }
    }
    Button { objectName: "batchApplyPreset"; text: "Применить"; enabled: row.ready && presetChoice.currentIndex >= 0; onClicked: batch.applyPreset(presets.items[presetChoice.currentIndex].id) }
    Button { objectName: "batchPaste"; text: "Paste"; enabled: row.ready && presets.hasCopiedSettings; onClicked: batch.paste() }
    Button { objectName: "batchSync"; text: "Sync Settings…"; enabled: row.ready && editor.hasPhoto; onClicked: syncDialog.open() }
    Item { Layout.fillWidth: true }
    Button { text: "Undo batch"; enabled: batch.canUndo && !library.working && !editor.busy && !editor.exporting; onClicked: batch.undo() }
    Button { text: "Redo batch"; enabled: batch.canRedo && !library.working && !editor.busy && !editor.exporting; onClicked: batch.redo() }
    Dialog {
        id: syncDialog; objectName: "batchSyncDialog"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: 430; implicitWidth: 430; modal: true
        title: "Sync Settings"
        property int groups: 127
        ColumnLayout {
            width: parent.width
            Label { text: "Источник: " + editor.filename; textFormat: Text.PlainText; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: "Выбрано фото: " + library.selectedCount + ". Изменятся только отмеченные группы."; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Repeater {
                model: ["Light", "Color", "Curve", "HSL", "Detail", "Crop", "Optics"]
                CheckBox {
                    required property string modelData
                    required property int index
                    text: modelData; checked: (syncDialog.groups & (1 << index)) !== 0
                    onClicked: syncDialog.groups = checked ? syncDialog.groups | (1 << index) : syncDialog.groups & ~(1 << index)
                }
            }
        }
        footer: DialogButtonBox {
            Button { text: "Отмена"; onClicked: syncDialog.close(); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { objectName: "batchSyncConfirm"; text: "Применить к выборке"; enabled: syncDialog.groups !== 0 && row.ready; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole; onClicked: { batch.sync(syncDialog.groups); syncDialog.close() } }
        }
    }
}
