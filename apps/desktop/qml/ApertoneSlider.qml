import QtQuick
import QtQuick.Controls

Slider {
    id: control

    implicitHeight: Theme.sliderHeight
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 120
        implicitHeight: Theme.sliderTrackHeight
        width: control.availableWidth
        height: Theme.sliderTrackHeight
        radius: height / 2
        color: Theme.divider
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: Theme.sliderHandleSize
        implicitHeight: Theme.sliderHandleSize
        radius: width / 2
        color: control.pressed ? Theme.accent : control.hovered ? Theme.controlHover : Theme.panelRaised
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus || control.pressed ? Theme.accent : Theme.muted
    }
}
