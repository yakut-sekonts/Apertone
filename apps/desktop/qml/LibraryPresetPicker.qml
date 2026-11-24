import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: picker
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    spacing: Theme.panelSpacing
    enabled: library.selectedCount > 0 && !library.working && !library.selecting && !editor.busy && !editor.exporting
    readonly property var choices: presets.builtinItems.map(item => ({ id: item.id, displayName: "◆ " + qsTranslate("Apertone", "Built-in") + " · " + item.displayName }))
                                   .concat(presets.items.map(item => ({ id: item.id, displayName: "● " + qsTranslate("Apertone", "User") + " · " + item.displayName })))

    ApertoneComboBox {
        id: choice
        objectName: "batchPresetChoice"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        model: picker.choices
        textRole: "displayName"
        enabled: count > 0
        Accessible.name: qsTranslate("Apertone", "Develop preset")
        delegate: ItemDelegate {
            required property var modelData
            width: choice.width
            contentItem: Label { text: modelData.displayName; textFormat: Text.PlainText; elide: Text.ElideMiddle }
            ToolTip.visible: hovered; ToolTip.text: modelData.displayName
        }
        contentItem: Label {
            text: choice.currentText; textFormat: Text.PlainText
            verticalAlignment: Text.AlignVCenter; elide: Text.ElideMiddle
        }
        ToolTip.visible: (hovered || visualFocus) && currentText.length > 0
        ToolTip.text: currentText
    }
    ApertoneButton {
        objectName: "batchApplyPreset"
        text: qsTranslate("Apertone", "Apply to selection")
        enabled: choice.currentIndex >= 0
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        onClicked: batch.applyPreset(picker.choices[choice.currentIndex].id)
    }
}
