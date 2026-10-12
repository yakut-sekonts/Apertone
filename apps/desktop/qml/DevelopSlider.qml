import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: control
    required property string parameter
    required property string label
    property real minimum: -100
    property real maximum: 100
    property real step: 1
    property real defaultValue: 0
    property string suffix: ""
    property string correction: parameter
    property bool allowDisable: true
    property bool available: editor.hasPhoto && !editor.busy
    spacing: 2
    RowLayout {
        Layout.fillWidth: true
        CheckBox {
            visible: control.allowDisable
            checked: editor.correctionStates[control.correction] ?? true
            enabled: control.available
            Accessible.name: "Включить " + control.label
            onToggled: editor.setCorrectionEnabled(control.correction, checked)
            padding: 0
        }
        Label { text: control.label; color: "#d7dce3"; font.pixelSize: 12; Layout.fillWidth: true }
        Label {
            text: Number(editor.parameters[control.parameter]).toFixed(control.step < 1 ? 2 : 0) + control.suffix
            color: "#dfae77"; font.pixelSize: 12
        }
    }
    Slider {
        Layout.fillWidth: true
        from: control.minimum; to: control.maximum; stepSize: control.step
        value: editor.parameters[control.parameter]
        enabled: control.available && (editor.correctionStates[control.correction] ?? true)
        Accessible.name: control.label
        onPressedChanged: { if (pressed) editor.beginEdit(); else editor.endEdit() }
        onMoved: editor.setParameter(control.parameter, value)
        TapHandler {
            acceptedButtons: Qt.LeftButton
            onDoubleTapped: { editor.endEdit(); editor.setParameter(control.parameter, control.defaultValue) }
        }
    }
}
