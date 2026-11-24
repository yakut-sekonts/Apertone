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
    property bool onCanvas: false
    property real divider: 0.5
    property bool syncing: false
    readonly property real viewWidth: canvas.width
    readonly property real viewHeight: canvas.height
    readonly property rect visibleSourceRect: Qt.rect(
        Math.max(0, (canvas.contentX - afterImage.x) / Math.max(1, afterImage.width)),
        Math.max(0, (canvas.contentY - afterImage.y) / Math.max(1, afterImage.height)),
        Math.min(1, canvas.width / Math.max(1, afterImage.width)),
        Math.min(1, canvas.height / Math.max(1, afterImage.height)))
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
        Label { text: qsTranslate("Apertone", "Before"); color: root.onCanvas ? Theme.canvasText : Theme.text; Layout.fillWidth: true }
        Label { text: qsTranslate("Apertone", "After"); color: root.onCanvas ? Theme.canvasText : Theme.text; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
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
                objectName: root.objectName + ".afterImage"
                source: root.afterSource
                cache: false
                asynchronous: true
                width: implicitWidth * canvas.imageScale
                height: implicitHeight * canvas.imageScale
                x: Math.max(0, (canvas.width - width) / 2)
                y: Math.max(0, (canvas.height - height) / 2)
                smooth: !root.pixelScale || Math.abs(root.zoom * Screen.devicePixelRatio - 1) > 0.001
            }
        }
        Rectangle {
            id: beforeClip
            width: Math.max(0, Math.min(viewport.width, root.divider * viewport.width))
            height: viewport.height
            clip: true
            z: 2
            color: root.onCanvas ? Theme.canvas : Theme.window
            ComparePane {
                objectName: root.objectName + ".beforePane"
                width: viewport.width; height: viewport.height
                title: ""
                source: root.beforeSource
                zoom: root.zoom; pixelScale: root.pixelScale
                centerX: root.centerX; centerY: root.centerY
                enabled: false
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
