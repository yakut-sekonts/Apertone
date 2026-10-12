import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    property string label
    property string value
    spacing: 12
    Label { text: parent.label; color: "#858b96"; font.pixelSize: 11; Layout.preferredWidth: 74 }
    Label { text: parent.value; color: "#d9dce2"; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
}
