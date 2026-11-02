import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApertoneButton {
    id: bell
    objectName: "updates.bell"
    implicitWidth: 32
    implicitHeight: Theme.controlHeight
    Accessible.name: updates.availableVersion.length ? qsTranslate("Apertone", "Apertone update available ")
                                                       + updates.availableVersion :
                                                       qsTranslate("Apertone", "Apertone updates")
    ToolTip.visible: hovered
    ToolTip.text: Accessible.name
    onClicked: notifications.open()
    contentItem: Item {
        Canvas {
            anchors.centerIn: parent
            width: 18
            height: 20
            onPaint: {
                const c = getContext("2d");
                c.clearRect(0, 0, width, height);
                c.strokeStyle = Theme.text;
                c.lineWidth = 1.6;
                c.beginPath();
                c.moveTo(2, 15);
                c.lineTo(4, 13);
                c.lineTo(4, 8);
                c.bezierCurveTo(4, 2, 14, 2, 14, 8);
                c.lineTo(14, 13);
                c.lineTo(16, 15);
                c.closePath();
                c.stroke();
                c.beginPath();
                c.arc(9, 17, 1.5, 0, Math.PI);
                c.stroke();
            }
        }
        Rectangle {
            objectName: "updates.availableBadge"
            visible: updates.availableVersion.length > 0
            width: 7
            height: 7
            radius: 3.5
            color: Theme.accent
            anchors.right: parent.right
            anchors.top: parent.top
        }
    }
    Popup {
        id: notifications
        objectName: "updates.popup"
        focus: true
        x: bell.width - width
        y: bell.height + 8
        width: 340
        padding: 14
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        ColumnLayout {
            width: parent.width
            spacing: Theme.panelSpacing
            Label {
                text: qsTranslate("Apertone", "Updates")
                font.pixelSize: 17
                font.bold: true
            }
            Label {
                visible: updates.availableVersion.length > 0
                text: "Apertone " + updates.availableVersion
                color: Theme.accent
                font.pixelSize: 15
            }
            Label {
                objectName: "updates.message"
                text: qsTranslate("Apertone", updates.message)
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            ApertoneButton {
                visible: updates.availableVersion.length > 0
                text: qsTranslate("Apertone", "Install update…")
                enabled: !updates.restartPending
                Layout.fillWidth: true
                onClicked: {
                    notifications.close();
                    updates.install();
                }
            }
            ApertoneButton {
                objectName: "updates.check"
                text: updates.checking ? qsTranslate("Apertone", "Checking…") : qsTranslate("Apertone", "Check for updates")
                enabled: updates.canCheck && !updates.checking
                Layout.fillWidth: true
                onClicked: updates.check()
            }
            ApertoneCheckBox {
                text: qsTranslate("Apertone", "Check automatically")
                checked: updates.automatic
                onClicked: updates.automatic = checked
            }
        }
    }
}
