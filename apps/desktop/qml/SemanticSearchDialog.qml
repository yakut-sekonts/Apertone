import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    objectName: "semantic.dialog"
    title: qsTranslate("Apertone", "Search by description")
    modal: true
    anchors.centerIn: parent
    width: Math.min(parent.width - 40, 900)
    height: Math.min(parent.height - 40, 650)
    standardButtons: Dialog.Close
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label {
            text: qsTranslate("Apertone", "Local search uses recognized objects and scenes. Enter a description in English.")
            wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            ApertoneButton {
                objectName: "semantic.build"
                text: library.semanticEnabled ? qsTranslate("Apertone", "Update local index") : qsTranslate("Apertone", "Enable and build index")
                enabled: !library.working
                onClicked: library.buildSemanticIndex()
            }
            ApertoneButton {
                objectName: "semantic.disable"
                text: qsTranslate("Apertone", "Disable and delete index")
                enabled: library.semanticEnabled
                onClicked: library.disableSemantic()
            }
            ApertoneButton {
                visible: library.semanticBusy && library.semanticEnabled
                text: qsTranslate("Apertone", "Cancel")
                onClicked: library.cancelSemantic()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            ApertoneTextField {
                id: query
                objectName: "semantic.query"
                placeholderText: qsTranslate("Apertone", "Describe a photo in English")
                maximumLength: 512; selectByMouse: true
                Layout.fillWidth: true
                enabled: library.semanticEnabled && !library.semanticBusy
                onAccepted: library.searchSemantic(text)
            }
            ApertoneButton {
                objectName: "semantic.search"
                text: qsTranslate("Apertone", "Search")
                enabled: library.semanticEnabled && !library.semanticBusy && query.text.trim().length > 0
                onClicked: library.searchSemantic(query.text)
            }
        }
        ProgressBar { visible: library.semanticBusy; value: library.semanticProgress; Layout.fillWidth: true }
        Label { text: library.semanticStatus; textFormat: Text.PlainText; Layout.fillWidth: true }
        Label { text: qsTranslate("Apertone", library.semanticError); visible: text.length > 0; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Theme.negative; Layout.fillWidth: true }
        GridView {
            id: results
            model: library.semanticResults
            currentIndex: -1
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            cellWidth: width / Math.max(1, Math.floor(width / 180))
            cellHeight: 170
            ScrollBar.vertical: ScrollBar {}
            delegate: Item {
                required property var modelData
                width: results.cellWidth; height: results.cellHeight
                ApertoneButton {
                    objectName: "semantic.result." + modelData.id
                    anchors.fill: parent; anchors.margins: 5
                    Accessible.name: modelData.filename
                    onClicked: { editor.openFile(modelData.url); dialog.close() }
                    contentItem: ColumnLayout {
                        Image {
                            source: modelData.thumbnail
                            sourceSize: Qt.size(384,384)
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            Layout.fillWidth: true; Layout.fillHeight: true
                        }
                        Label { text: modelData.filename; textFormat: Text.PlainText; elide: Text.ElideMiddle; Layout.fillWidth: true }
                    }
                }
            }
        }
    }
}
