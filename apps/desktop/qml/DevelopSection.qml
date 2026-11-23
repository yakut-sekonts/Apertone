import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: section
    required property string title
    property bool expanded: true
    default property alias controls: body.data
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    spacing: Theme.panelSpacing

    function focusHeader() { header.forceActiveFocus(Qt.TabFocusReason) }

    ApertoneButton {
        id: header
        objectName: section.objectName + ".toggle"
        text: section.title
        checkable: true
        checked: section.expanded
        onToggled: section.expanded = checked
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        background: Rectangle {
            implicitHeight: Theme.controlHeight
            radius: Theme.radius
            color: header.down ? Theme.control : header.hovered ? Theme.controlHover : Theme.panelRaised
            border.width: header.visualFocus ? 2 : 1
            border.color: header.visualFocus ? Theme.accent : Theme.divider
        }
        contentItem: RowLayout {
            Label {
                text: header.text
                color: Theme.text
                font: header.font
                elide: Text.ElideRight
                Layout.fillWidth: true
                Accessible.ignored: true
            }
            Label {
                text: section.expanded ? "▾" : "▸"
                color: Theme.muted
                Accessible.ignored: true
            }
        }
    }

    ColumnLayout {
        id: body
        visible: section.expanded
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: Theme.panelSpacing
    }
}
