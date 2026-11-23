import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: dialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: parent.width - 32
    height: parent.height - 32
    title: editor.comparison.beforeAfter
           ? qsTranslate("Apertone", "Before / After")
           : qsTranslate("Apertone", "Compare Select / Candidate")
    property real centerX: 0.5
    property real centerY: 0.5
    property bool swapped: false
    property string selectID: ""
    property var candidateIDs: []
    property int candidateIndex: 0
    function showCandidates(select, candidates) {
        selectID = select
        candidateIDs = candidates
        candidateIndex = 0
        editor.prepareCompare([selectID, candidateIDs[candidateIndex]])
        open()
    }
    function moveCandidate(direction) {
        const target = candidateIndex + direction
        if (target < 0 || target >= candidateIDs.length)
            return
        candidateIndex = target
        swapped = false
        editor.prepareCompare([selectID, candidateIDs[target]], editor.comparison.fullResolution)
    }
    onOpened: { centerX = 0.5; centerY = 0.5; swapped = false; zoomControl.value = 1 }
    onClosed: editor.closeCompare()
    Shortcut {
        sequence: "Down"
        enabled: dialog.visible && !editor.comparison.beforeAfter && editor.comparison.ready
        onActivated: dialog.swapped = !dialog.swapped
    }
    Shortcut {
        sequence: "Left"
        enabled: dialog.visible && dialog.candidateIndex > 0
        onActivated: dialog.moveCandidate(-1)
    }
    Shortcut {
        sequence: "Right"
        enabled: dialog.visible && dialog.candidateIndex + 1 < dialog.candidateIDs.length
        onActivated: dialog.moveCandidate(1)
    }
    ColumnLayout {
        anchors.fill: parent
        RowLayout {
            ApertoneButton {
                text: qsTranslate("Apertone", "Fit")
                onClicked: {
                    zoomControl.value = 1; dialog.centerX = 0.5; dialog.centerY = 0.5
                    if (editor.comparison.fullResolution)
                        editor.comparison.beforeAfter
                            ? editor.prepareBeforeAfter(false)
                            : editor.prepareCompare(editor.comparison.ids, false)
                }
            }
            ApertoneButton {
                text: "100%"
                Accessible.name: qsTranslate("Apertone", "Zoom") + " 100%"
                enabled: !editor.comparison.busy
                onClicked: {
                    zoomControl.value = 1
                    editor.comparison.beforeAfter
                        ? editor.prepareBeforeAfter(true)
                        : editor.prepareCompare(editor.comparison.ids, true)
                }
            }
            Label { text: qsTranslate("Apertone", "Linked zoom and pan") }
            ApertoneSlider {
                id: zoomControl
                from: 1; to: 4; value: 1
                Layout.fillWidth: true
                enabled: editor.comparison.ready
                Accessible.name: qsTranslate("Apertone", "Zoom")
            }
            ApertoneButton {
                visible: !editor.comparison.beforeAfter
                text: qsTranslate("Apertone", "Swap")
                enabled: editor.comparison.ready
                onClicked: dialog.swapped = !dialog.swapped
            }
            ApertoneButton {
                objectName: "library.compare.previous"
                visible: dialog.candidateIDs.length > 1
                enabled: dialog.candidateIndex > 0
                text: "‹"
                Accessible.name: qsTranslate("Apertone", "Previous")
                onClicked: dialog.moveCandidate(-1)
            }
            ApertoneButton {
                objectName: "library.compare.next"
                visible: dialog.candidateIDs.length > 1
                enabled: dialog.candidateIndex + 1 < dialog.candidateIDs.length
                text: "›"
                Accessible.name: qsTranslate("Apertone", "Next")
                onClicked: dialog.moveCandidate(1)
            }
            ApertoneButton { text: qsTranslate("Apertone", "Close"); onClicked: dialog.close() }
        }
        BusyIndicator { running: editor.comparison.busy; visible: running; Layout.alignment: Qt.AlignHCenter }
        Label { visible: editor.comparison.error.length > 0; text: qsTranslate("Apertone", editor.comparison.error); wrapMode: Text.Wrap; Layout.fillWidth: true }
        SplitComparePane {
            visible: editor.comparison.ready && editor.comparison.beforeAfter
            Layout.fillWidth: true
            Layout.fillHeight: true
            beforeSource: editor.comparison.ready ? editor.comparison.left : ""
            afterSource: editor.comparison.ready ? editor.comparison.right : ""
            zoom: zoomControl.value
            centerX: dialog.centerX
            centerY: dialog.centerY
            pixelScale: editor.comparison.fullResolution
            onPanned: function(x, y) { dialog.centerX = x; dialog.centerY = y }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: editor.comparison.ready && !editor.comparison.beforeAfter
            Repeater {
                model: 2
                delegate:ComparePane {
                    required property int index
                    property int side: dialog.swapped ? 1 - index : index
                    Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 1
                    source: editor.comparison.ready ? (side === 0 ? editor.comparison.left : editor.comparison.right) : ""
                    title: (index === 0 ? qsTranslate("Apertone", "Select") : qsTranslate("Apertone", "Candidate")) + " · " + (editor.comparison.names[side] ?? "")
                    zoom: zoomControl.value; centerX: dialog.centerX; centerY: dialog.centerY; pixelScale: editor.comparison.fullResolution
                    onPanned: function(x, y) { dialog.centerX = x; dialog.centerY = y }
                }
            }
        }
    }
}
