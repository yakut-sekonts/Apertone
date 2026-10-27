import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    property string label
    property string value
    spacing: 12
    Label { text: parent.label; color: Theme.muted; font.pixelSize: 11; Layout.preferredWidth: 74 }
    Label { text: parent.value; color: Theme.text; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
}
