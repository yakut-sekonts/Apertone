import QtQuick
import QtQuick.Controls

TextField {
    id: control

    implicitHeight: Theme.controlHeight
    leftPadding: 7
    rightPadding: 7
    topPadding: 3
    bottomPadding: 3
    font.pixelSize: 11
    color: Theme.text
    placeholderTextColor: Theme.faint
    selectionColor: Theme.accent
    selectedTextColor: "#ffffff"

    background: Rectangle {
        implicitHeight: Theme.controlHeight
        radius: Theme.radius
        color: Theme.panelRaised
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.divider
    }
}
