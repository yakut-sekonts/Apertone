import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    implicitHeight: Theme.controlHeight
    leftPadding: 9
    rightPadding: 28
    topPadding: 5
    bottomPadding: 5
    font.pixelSize: 11

    palette.text: Theme.text
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: "#ffffff"

    background: Rectangle {
        implicitHeight: Theme.controlHeight
        radius: Theme.radius
        color: control.down ? Theme.divider : control.hovered ? Theme.controlHover : Theme.control
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.divider
    }
}
