import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    objectName: "library.survey"
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: parent.width - 32
    height: parent.height - 32
    title: qsTranslate("Apertone", "Survey")
    standardButtons: Dialog.Close
    property var excluded: []
    readonly property var comparison: editor.comparison
    readonly property var visibleTiles: {
        if (!comparison.ready)
            return []
        return comparison.ids.map((id, index) => ({
            "id": id, "name": comparison.names[index],
            "url": comparison.sources[index], "error": comparison.errors[index]
        })).filter(tile => !excluded.includes(tile.id))
    }
    function showSelection() {
        excluded = []
        editor.prepareSurvey(library.selectedAssetIDs())
        open()
    }
    onClosed: editor.closeCompare()

    contentItem: Item {
        BusyIndicator {
            anchors.centerIn: parent
            running: dialog.visible && dialog.comparison.busy
            visible: running
        }
        Label {
            anchors.centerIn: parent
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            visible: dialog.comparison.error.length > 0
            text: qsTranslate("Apertone", dialog.comparison.error)
        }
        GridLayout {
            anchors.fill: parent
            columns: Math.min(Math.max(1, Math.floor(width / 210)),
                              Math.max(1, Math.ceil(Math.sqrt(dialog.visibleTiles.length))))
            columnSpacing: Theme.panelSpacing
            rowSpacing: Theme.panelSpacing
            Repeater {
                model: dialog.visible ? dialog.visibleTiles : []
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 1
                    Layout.minimumWidth: 0
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Image {
                            objectName: "library.surveyPreview." + modelData.id
                            anchors.fill: parent
                            source: modelData.error ? "" : modelData.url
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            cache: false
                            Accessible.name: modelData.name
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            color: Theme.muted
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            visible: Boolean(modelData.error)
                            text: qsTranslate("Apertone", modelData.error || "")
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: modelData.name
                            textFormat: Text.PlainText
                            elide: Text.ElideMiddle
                        }
                        ApertoneButton {
                            objectName: "library.surveyRemove." + modelData.id
                            text: "×"
                            Accessible.name: qsTranslate("Apertone", "Remove from selection")
                            ToolTip.text: Accessible.name
                            ToolTip.visible: hovered
                            enabled: dialog.visibleTiles.length > 1 && !library.working
                            onClicked: {
                                if (library.removeFromSelection(modelData.id))
                                    dialog.excluded = dialog.excluded.concat(modelData.id)
                            }
                        }
                    }
                }
            }
        }
    }
}
