import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: view
    property bool split: false
    property string mode: "fit"
    property real manualZoom: 1
    property real centerX: 0.5
    property real centerY: 0.5
    readonly property bool ready: visible && editor.comparison.ready && editor.comparison.beforeAfter
    readonly property string beforeSource: ready ? editor.comparison.left : ""
    readonly property string afterSource: ready ? (editor.comparison.fullResolution ? editor.comparison.right : editor.previewUrl) : ""
    readonly property var viewport: split ? wipe : afterPane
    readonly property rect visibleRect: viewport.visibleSourceRect
    readonly property real horizontalScale: viewport.viewWidth * Screen.devicePixelRatio / Math.max(1, editor.previewPixelSize.width)
    readonly property real verticalScale: viewport.viewHeight * Screen.devicePixelRatio / Math.max(1, editor.previewPixelSize.height)
    readonly property real effectiveZoom: mode === "fit" ? Math.min(horizontalScale, verticalScale)
        : mode === "fill" ? Math.max(horizontalScale, verticalScale) : manualZoom
    readonly property real paneZoom: mode === "fit" ? 1 : effectiveZoom / Screen.devicePixelRatio

    function setZoom(nextMode, factor = 1) {
        mode = nextMode
        manualZoom = factor
        if (nextMode === "fit") panTo(0.5, 0.5)
        if (editor.comparison.fullResolution !== (nextMode !== "fit"))
            editor.prepareBeforeAfter(nextMode !== "fit")
    }
    function panTo(x, y) {
        centerX = Math.max(0, Math.min(1, x))
        centerY = Math.max(0, Math.min(1, y))
    }
    onVisibleChanged: {
        if (!visible) {
            mode = "fit"; manualZoom = 1; split = false; panTo(0.5, 0.5)
        }
    }

    component PhotoPane: ComparePane {
        onCanvas: true
        Layout.fillWidth: true; Layout.fillHeight: true
        Layout.preferredWidth: 1; Layout.minimumWidth: 0
        zoom: view.paneZoom; pixelScale: view.mode !== "fit"
        centerX: view.centerX; centerY: view.centerY
        onPanned: function(x, y) { view.panTo(x, y) }
    }
    RowLayout {
        anchors.fill: parent
        visible: !view.split
        spacing: Theme.panelSpacing
        PhotoPane {
            objectName: "develop.comparison.before"
            title: qsTranslate("Apertone", "Before")
            source: visible ? view.beforeSource : ""
        }
        Rectangle { Layout.fillHeight: true; implicitWidth: 1; color: Theme.divider }
        PhotoPane {
            id: afterPane
            objectName: "develop.comparison.after"
            title: qsTranslate("Apertone", "After")
            source: visible ? view.afterSource : ""
        }
    }
    SplitComparePane {
        id: wipe
        onCanvas: true
        objectName: "develop.comparison.wipe"
        anchors.fill: parent
        visible: view.split
        beforeSource: visible ? view.beforeSource : ""
        afterSource: visible ? view.afterSource : ""
        zoom: view.paneZoom; pixelScale: view.mode !== "fit"
        centerX: view.centerX; centerY: view.centerY
        onPanned: function(x, y) { view.panTo(x, y) }
    }
    BusyIndicator { anchors.centerIn: parent; running: view.visible && editor.comparison.busy && !view.ready }
    Label {
        anchors.centerIn: parent; width: parent.width
        visible: view.visible && !!editor.comparison.error
        color: Theme.canvasText; horizontalAlignment: Text.AlignHCenter
        text: qsTranslate("Apertone", editor.comparison.error); wrapMode: Text.Wrap
    }
}
