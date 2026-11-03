import QtQuick
import QtQuick.Controls

Button {
    id: control

    implicitHeight: Theme.controlHeight
    leftPadding: 8
    rightPadding: 8
    topPadding: 3
    bottomPadding: 3
    spacing: 5
    font.pixelSize: 11
    font.weight: highlighted || checked ? Font.DemiBold : Font.Medium
    opacity: enabled ? 1 : 0.48

    palette.buttonText: highlighted || checked ? "#ffffff" : Theme.text
    palette.highlightedText: "#ffffff"

    background: Rectangle {
        implicitHeight: Theme.controlHeight
        radius: Theme.radius
        color: control.highlighted || control.checked
               ? Theme.accent
               : control.down
                 ? Theme.divider
                 : control.hovered
                   ? Theme.controlHover
                   : control.flat
                     ? "transparent"
                     : Theme.control
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus || control.highlighted || control.checked
                      ? Theme.accent
                      : control.flat
                        ? "transparent"
                        : Theme.divider
    }
}
