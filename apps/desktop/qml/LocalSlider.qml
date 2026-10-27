import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: control
    Layout.preferredWidth: 200
    required property string parameter
    required property string label
    property real minimum: -100
    property real maximum: 100
    property real step: 1
    Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: parent.width
    spacing: 2
    RowLayout {
        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: control.width
        Label { text: control.label; wrapMode: Text.Wrap; color: Theme.text; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: control.width; font.pixelSize: 12 }
        Label { text: Number(editor.localState[control.parameter] ?? 0).toFixed(control.step < 1 ? 2 : 0); color: Theme.accent; font.pixelSize: 12 }
    }
    Slider {
        objectName: "local_" + control.parameter
        Accessible.name: control.label
        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: control.width
        enabled: editor.hasPhoto && !editor.busy && !editor.editingLocked && !!editor.localState.enabled
        from: control.minimum; to: control.maximum; stepSize: control.step
        value: editor.localState[control.parameter] ?? 0
        onPressedChanged: { if (pressed) editor.beginEdit(); else editor.endEdit() }
        onMoved: editor.setLocalParameter(control.parameter, value)
    }
}
