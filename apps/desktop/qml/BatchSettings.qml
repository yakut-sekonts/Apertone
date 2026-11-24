import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GridLayout {
    id: row
    columns: 2
    uniformCellWidths: true
    columnSpacing: 6
    rowSpacing: 6
    objectName: "batchSettings"
    Layout.minimumWidth: 0
    Layout.fillWidth: true
    property bool ready: library.selectedCount > 0 && !library.working && !library.selecting && !editor.busy && !editor.exporting
    ApertoneButton {
        objectName: "batchPaste"
        Layout.minimumWidth: 0; Layout.fillWidth: true
        Layout.columnSpan: 2
        ToolTip.visible: hovered; ToolTip.text: text
        text: qsTranslate("Apertone", "Paste")
        enabled: row.ready && presets.hasCopiedSettings
        onClicked: batch.paste()
    }
    ApertoneButton {
        objectName: "batchSync"
        Layout.minimumWidth: 0; Layout.fillWidth: true
        Layout.columnSpan: 2
        ToolTip.visible: hovered; ToolTip.text: text
        text: qsTranslate("Apertone", "Sync Settings…")
        enabled: row.ready && editor.hasPhoto
        onClicked: syncDialog.open()
    }
    ApertoneButton {
        objectName: "batchUndo"
        Layout.minimumWidth: 0; Layout.fillWidth: true
        text: qsTranslate("Apertone", "Undo")
        Accessible.name: qsTranslate("Apertone", "Undo batch")
        ToolTip.visible: hovered; ToolTip.text: Accessible.name
        enabled: batch.canUndo && !library.working && !editor.busy && !editor.exporting
        onClicked: batch.undo()
    }
    ApertoneButton {
        objectName: "batchRedo"
        Layout.minimumWidth: 0; Layout.fillWidth: true
        text: qsTranslate("Apertone", "Redo")
        Accessible.name: qsTranslate("Apertone", "Redo batch")
        ToolTip.visible: hovered; ToolTip.text: Accessible.name
        enabled: batch.canRedo && !library.working && !editor.busy && !editor.exporting
        onClicked: batch.redo()
    }
    Dialog {
        id: syncDialog; objectName: "batchSyncDialog"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: 430; implicitWidth: 430; modal: true
        title: qsTranslate("Apertone", "Sync Settings")
        property int groups: 32767
        contentItem: ColumnLayout {
            Label { text: qsTranslate("Apertone", "Source: ") + editor.filename; textFormat: Text.PlainText; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: qsTranslate("Apertone", "Selected photos: ") + library.selectedCount + qsTranslate("Apertone", ". Only the selected groups will change."); wrapMode: Text.Wrap; Layout.fillWidth: true }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                uniformCellWidths: true
                columnSpacing: Theme.panelSpacing
                rowSpacing: Theme.panelSpacing
                Repeater {
                    model: [
                        qsTranslate("Apertone", "Light"), qsTranslate("Apertone", "Color"),
                        qsTranslate("Apertone", "Curve"), "HSL", qsTranslate("Apertone", "Detail"),
                        qsTranslate("Apertone", "Crop"), qsTranslate("Apertone", "Optics"),
                        qsTranslate("Apertone", "Geometry"), qsTranslate("Apertone", "Presence"),
                        qsTranslate("Apertone", "Point Color"), qsTranslate("Apertone", "Color Grading"),
                        qsTranslate("Apertone", "Effects"), qsTranslate("Apertone", "Black and White"),
                        qsTranslate("Apertone", "Defringe"), qsTranslate("Apertone", "Calibration")
                    ]
                    ApertoneCheckBox {
                        required property string modelData
                        required property int index
                        objectName: "batchSyncGroup" + index
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        text: modelData; checked: (syncDialog.groups & (1 << index)) !== 0
                        onClicked: syncDialog.groups = checked ? syncDialog.groups | (1 << index) : syncDialog.groups & ~(1 << index)
                    }
                }
            }
        }
        footer: DialogButtonBox {
            ApertoneButton { text: qsTranslate("Apertone", "Cancel"); onClicked: syncDialog.close(); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            ApertoneButton { objectName: "batchSyncConfirm"; text: qsTranslate("Apertone", "Apply to selection"); enabled: syncDialog.groups !== 0 && row.ready; DialogButtonBox.buttonRole: DialogButtonBox.ActionRole; onClicked: { batch.sync(syncDialog.groups); syncDialog.close() } }
        }
    }
}
