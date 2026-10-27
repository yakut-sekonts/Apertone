import QtQuick
import QtQuick.Controls

SpinBox {
    id: control

    implicitHeight: Theme.controlHeight
    font.pixelSize: 11
    palette.text: Theme.text
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: "#ffffff"

    background: Rectangle {
        implicitHeight: Theme.controlHeight
        radius: Theme.radius
        color: Theme.panelRaised
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.divider
    }
}
