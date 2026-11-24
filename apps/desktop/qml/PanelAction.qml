import QtQuick
import QtQuick.Layouts

ApertoneButton {
    id: control
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    implicitHeight: Math.max(Theme.controlHeight, caption.implicitHeight + topPadding + bottomPadding)
    contentItem: Text {
        id: caption
        text: control.text
        font: control.font
        color: control.highlighted || control.checked ? "#ffffff" : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
    }
}
