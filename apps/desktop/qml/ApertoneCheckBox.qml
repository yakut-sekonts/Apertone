import QtQuick
import QtQuick.Controls

CheckBox {
    id: control

    implicitHeight: Math.max(Theme.checkHeight, contentItem.implicitHeight)
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    spacing: 6
    font.pixelSize: 11
    hoverEnabled: true
    opacity: enabled ? 1 : 0.48

    indicator: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: Theme.checkIndicatorSize
        implicitHeight: Theme.checkIndicatorSize
        radius: 2
        color: control.checked || control.checkState === Qt.PartiallyChecked
               ? Theme.accent
               : control.hovered
                 ? Theme.controlHover
                 : Theme.panelRaised
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.divider

        Text {
            anchors.centerIn: parent
            text: control.checkState === Qt.PartiallyChecked ? "−" : "✓"
            font.pixelSize: 9
            font.weight: Font.Bold
            color: "#ffffff"
            visible: control.checked || control.checkState === Qt.PartiallyChecked
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: control.enabled ? Theme.text : Theme.faint
        wrapMode: Text.Wrap
        verticalAlignment: Text.AlignVCenter
    }
}
