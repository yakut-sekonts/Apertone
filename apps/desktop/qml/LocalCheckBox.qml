import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

CheckBox {
    id: control
    Layout.preferredWidth: 200
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    Layout.maximumWidth: parent.width
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? "#d7dce3" : "#626771"
        wrapMode: Text.Wrap
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator ? control.indicator.width + control.spacing : 0
    }
}
