import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    ApertoneButton {
        objectName: "retouch.toggle"
        text: qsTranslate("Apertone", "Retouch")
        checkable: true
        checked: editor.retouchMode !== "off"
        enabled: editor.hasPhoto && !editor.busy && !editor.editingLocked && !editor.original
        onClicked: editor.retouchMode = checked ? "heal" : "off"
        Layout.fillWidth: true
    }
    ColumnLayout {
        visible: editor.retouchMode !== "off"
        enabled: editor.hasPhoto && !editor.busy && !editor.editingLocked && !editor.original
        Layout.fillWidth: true
        RowLayout {
            Layout.fillWidth: true
            Repeater {
                model: [{key: "clone", title: qsTranslate("Apertone", "Clone")},
                        {key: "heal", title: qsTranslate("Apertone", "Heal")},
                        {key: "remove", title: qsTranslate("Apertone", "Remove")}]
                ApertoneButton {
                    required property var modelData
                    objectName: "retouch.mode." + modelData.key
                    text: modelData.title
                    highlighted: editor.retouchMode === modelData.key
                    onClicked: editor.retouchMode = modelData.key
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                }
            }
        }
        Label {
            text: editor.retouchMode === "remove"
                ? qsTranslate("Apertone", "Paint over a small object to find a replacement area.")
                : qsTranslate("Apertone", "Option-click a source area, then paint over the target.")
            wrapMode: Text.Wrap; font.pixelSize: 11; color: Theme.muted
            Layout.fillWidth: true
        }
        Repeater {
            model: [{key: "radius", title: qsTranslate("Apertone", "Size"), min: .1, max: 25},
                    {key: "feather", title: qsTranslate("Apertone", "Feather"), min: 0, max: 100},
                    {key: "opacity", title: qsTranslate("Apertone", "Opacity"), min: 0, max: 100}]
            ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                Label { text: modelData.title + " · " + Number(editor.retouchState[modelData.key]).toFixed(1) }
                Slider {
                    Layout.fillWidth: true
                    from: modelData.min; to: modelData.max
                    value: editor.retouchState[modelData.key]
                    enabled: !editor.retouchBusy
                    Accessible.name: modelData.title
                    onPressedChanged: if (pressed) editor.beginEdit(); else editor.endEdit()
                    onMoved: editor.setRetouchControl(modelData.key, value)
                }
            }
        }
        ApertoneComboBox {
            id: strokes
            Layout.fillWidth: true
            model: editor.retouchItems
            textRole: "number"
            Accessible.name: qsTranslate("Apertone", "Retouch stroke")
            currentIndex: {
                for (let i = 0; i < model.length; ++i)
                    if (model[i].id === editor.retouchState.selected) return i
                return -1
            }
            onActivated: editor.selectRetouch(model[currentIndex].id)
        }
        RowLayout {
            Layout.fillWidth: true
            ApertoneButton { text: qsTranslate("Apertone", "New stroke"); onClicked: editor.selectRetouch(""); Layout.fillWidth: true }
            ApertoneButton {
                text: qsTranslate("Apertone", "Delete")
                enabled: editor.retouchState.selected.length > 0
                onClicked: editor.removeRetouch()
                Layout.fillWidth: true
            }
        }
        CheckBox {
            visible: editor.retouchState.selected.length > 0
            text: qsTranslate("Apertone", "Enabled")
            checked: editor.retouchState.enabled
            onToggled: editor.setRetouchEnabled(checked)
        }
        Label {
            visible: editor.retouchState.selected.length > 0
            text: qsTranslate("Apertone", "Option-click to reposition the selected source.")
            wrapMode: Text.Wrap; font.pixelSize: 11; color: Theme.muted
            Layout.fillWidth: true
        }
        ProgressBar { objectName: "retouch.progress"; visible: editor.retouchBusy; indeterminate: true; Layout.fillWidth: true }
        ApertoneButton {
            objectName: "retouch.cancel"
            visible: editor.retouchBusy || editor.retouchState.drawing
            text: qsTranslate("Apertone", "Cancel")
            onClicked: editor.cancelRetouch()
            Layout.fillWidth: true
        }
    }
}
