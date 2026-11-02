import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property string beforeSource
    required property string afterSource
    required property real zoom
    required property real centerX
    required property real centerY
    property bool pixelScale: false
    property real divider: 0.5
    property bool syncing: false
    signal panned(real x, real y)

    function position() {
        if (!canvas) return
        syncing = true
        canvas.contentX = Math.max(0, Math.min(canvas.contentWidth - canvas.width,
                                              centerX * canvas.contentWidth - canvas.width / 2))
        canvas.contentY = Math.max(0, Math.min(canvas.contentHeight - canvas.height,
                                              centerY * canvas.contentHeight - canvas.height / 2))
        syncing = false
    }
    function publish() {
        if (!syncing && canvas.dragging)
            panned(canvas.contentWidth > canvas.width
                       ? (canvas.contentX + canvas.width / 2) / canvas.contentWidth : centerX,
                   canvas.contentHeight > canvas.height
                       ? (canvas.contentY + canvas.height / 2) / canvas.contentHeight : centerY)
    }
    onCenterXChanged: position()
    onCenterYChanged: position()

    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTranslate("Apertone", "Before"); Layout.fillWidth: true }
        Label { text: qsTranslate("Apertone", "After"); Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
    }
    Item {
        id: viewport
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        Flickable {
            id: canvas
            anchors.fill: parent
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            property real imageScale: root.pixelScale ? root.zoom
                : Math.min(width / Math.max(1, afterImage.implicitWidth),
                           height / Math.max(1, afterImage.implicitHeight)) * root.zoom
            contentWidth: Math.max(width, afterImage.width)
            contentHeight: Math.max(height, afterImage.height)
            onContentWidthChanged: root.position()
            onContentHeightChanged: root.position()
            onContentXChanged: root.publish()
            onContentYChanged: root.publish()
            Image {
                id: afterImage
                source: root.afterSource
                cache: false
                asynchronous: true
                width: implicitWidth * canvas.imageScale
                height: implicitHeight * canvas.imageScale
                x: Math.max(0, (canvas.width - width) / 2)
                y: Math.max(0, (canvas.height - height) / 2)
                smooth: !root.pixelScale || root.zoom !== 1
            }
        }
        Item {
            id: beforeClip
            width: Math.max(0, Math.min(viewport.width, root.divider * viewport.width))
            height: viewport.height
            clip: true
            z: 2
            Image {
                source: root.beforeSource
                cache: false
                asynchronous: true
                width: afterImage.width
                height: afterImage.height
                x: afterImage.x - canvas.contentX
                y: afterImage.y - canvas.contentY
                smooth: afterImage.smooth
            }
        }
        Rectangle {
            x: beforeClip.width - width / 2
            width: 2
            height: parent.height
            color: Theme.accent
            z: 3
        }
        Rectangle {
            x: beforeClip.width - width / 2
            y: parent.height / 2 - height / 2
            width: 22
            height: 44
            radius: 11
            color: Theme.panelRaised
            border.color: Theme.accent
            z: 4
            Label { anchors.centerIn: parent; text: "↔"; color: Theme.text }
        }
        MouseArea {
            x: beforeClip.width - width / 2
            width: 36
            height: parent.height
            z: 5
            cursorShape: Qt.SplitHCursor
            onPressed: function(mouse) { mouse.accepted = true }
            onPositionChanged: function(mouse) {
                if (pressed && viewport.width > 0)
                    root.divider = Math.max(0.05, Math.min(0.95,
                        (x + mouse.x) / viewport.width))
            }
            Accessible.name: qsTranslate("Apertone", "Split comparison")
        }
    }
}
