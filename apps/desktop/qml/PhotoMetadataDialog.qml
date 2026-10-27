import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: dialog
    objectName: "photoMetadata"
    title: qsTranslate("Apertone", "Photo metadata")
    modal: true; anchors.centerIn: parent
    width: Math.min(parent.width - 40, 610)
    height: Math.min(parent.height - 40, 720)
    standardButtons: Dialog.Close
    readonly property bool ready: !!library.descriptiveMetadata.id && !library.working
    function showPhoto(id) { library.loadMetadata(id); open() }
    Connections {
        target: library
        function onMetadataChanged() {
            const values = library.descriptiveMetadata
            titleField.text = values.title || ""; captionField.text = values.caption || ""
            copyrightField.text = values.copyright || ""; creatorsField.text = (values.creators || []).join("\n")
            locationField.text = values.location || ""; keywordsField.text = (values.keywords || []).join("\n")
            ratingField.currentIndex = values.rating || 0
            flagField.currentIndex = (values.flag || 0) + 1
            labelField.text = values.label || ""
        }
    }
    FileDialog { id: locateFile; objectName: "metadata.locateFileDialog"; title: qsTranslate("Apertone", "Locate original"); onAccepted: library.locateOriginal(selectedFile) }
    FolderDialog { id: locateFolder; objectName: "metadata.locateFolderDialog"; title: qsTranslate("Apertone", "Locate folder"); onAccepted: library.locateFolder(selectedFolder, nearby.checked) }
    Dialog {
        id: confirmImport
        objectName: "metadata.confirmRead"
        title: qsTranslate("Apertone", "Read metadata from original and XMP?")
        modal: true; anchors.centerIn: parent; width: Math.min(dialog.width - 40, 480)
        implicitHeight: Math.min(dialog.height - 40, 260)
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: ScrollView {
            id: importWarning
            clip: true
            contentWidth: availableWidth
            Label {
                width: importWarning.availableWidth; wrapMode: Text.Wrap
                text: qsTranslate("Apertone", "Replaces the catalog metadata and this draft with the values from the original and sidecar. Develop edits remain unchanged.")
            }
        }
        onAccepted: library.importDescriptiveMetadata()
    }
    contentItem: ScrollView {
        objectName: "metadata.scroll"
        clip: true; contentWidth: availableWidth
        ColumnLayout {
            width: parent.width; spacing: 8
            Label { objectName: "metadata.path"; text: library.descriptiveMetadata.path || ""; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
            Label { text: qsTranslate("Apertone", library.descriptiveMetadata.readError || ""); textFormat: Text.PlainText; visible: text.length > 0; color: Theme.negative; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { objectName: "metadata.error"; text: qsTranslate("Apertone", library.error); textFormat: Text.PlainText; visible: text.length > 0; color: Theme.negative; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: qsTranslate("Apertone", "Title") }
            ApertoneTextField { id: titleField; objectName: "metadata.title"; enabled: dialog.ready; Layout.fillWidth: true; maximumLength: 1024; selectByMouse: true; Accessible.name: qsTranslate("Apertone", "Title") }
            Label { text: qsTranslate("Apertone", "Caption") }
            TextArea { id: captionField; objectName: "metadata.caption"; enabled: dialog.ready; Layout.fillWidth: true; Layout.minimumHeight: 75; wrapMode: Text.Wrap; selectByMouse: true; Accessible.name: qsTranslate("Apertone", "Caption") }
            Label { text: qsTranslate("Apertone", "Creators · one per line") }
            TextArea { id: creatorsField; objectName: "metadata.creators"; enabled: dialog.ready; Layout.fillWidth: true; Layout.minimumHeight: 65; textFormat: TextEdit.PlainText; wrapMode: Text.Wrap; selectByMouse: true; Accessible.name: qsTranslate("Apertone", "Creators · one per line") }
            Label { text: qsTranslate("Apertone", "Copyright") }
            ApertoneTextField { id: copyrightField; objectName: "metadata.copyright"; enabled: dialog.ready; Layout.fillWidth: true; maximumLength: 2048; selectByMouse: true; Accessible.name: qsTranslate("Apertone", "Copyright") }
            Label { text: qsTranslate("Apertone", "Location") }
            ApertoneTextField { id: locationField; objectName: "metadata.location"; enabled: dialog.ready; Layout.fillWidth: true; maximumLength: 2048; selectByMouse: true; Accessible.name: qsTranslate("Apertone", "Location") }
            Label { text: qsTranslate("Apertone", "Keywords · one per line") }
            TextArea { id: keywordsField; objectName: "metadata.keywords"; enabled: dialog.ready; Layout.fillWidth: true; Layout.minimumHeight: 75; wrapMode: Text.Wrap; selectByMouse: true; Accessible.name: qsTranslate("Apertone", "Keywords · one per line") }
            RowLayout {
                Layout.fillWidth: true
                ApertoneComboBox { id: ratingField; objectName: "metadata.rating"; enabled: dialog.ready; model: ["0 ★", "1 ★", "2 ★", "3 ★", "4 ★", "5 ★"]; Accessible.name: qsTranslate("Apertone", "Rating"); Layout.fillWidth: true }
                ApertoneComboBox { id: flagField; objectName: "metadata.flag"; enabled: dialog.ready; model: [qsTranslate("Apertone", "Reject"), qsTranslate("Apertone", "Unflagged"), qsTranslate("Apertone", "Pick")]; Accessible.name: qsTranslate("Apertone", "Flag"); Layout.fillWidth: true }
            }
            Label { text: qsTranslate("Apertone", "XMP stores Reject as -1 instead of a star rating. Pick stays in the catalog."); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Label { text: qsTranslate("Apertone", "XMP label") }
            RowLayout {
                Layout.fillWidth: true
                ApertoneTextField { id: labelField; objectName: "metadata.label"; enabled: dialog.ready; Layout.fillWidth: true; maximumLength: 1024; selectByMouse: true; Accessible.name: qsTranslate("Apertone", "XMP label") }
                ApertoneComboBox {
                    enabled: dialog.ready; Layout.preferredWidth: 160
                    model: [qsTranslate("Apertone", "No label"), qsTranslate("Apertone", "Red"), qsTranslate("Apertone", "Yellow"), qsTranslate("Apertone", "Green"), qsTranslate("Apertone", "Blue"), qsTranslate("Apertone", "Purple")]
                    currentIndex: ["", "red", "yellow", "green", "blue", "purple"].indexOf(labelField.text.toLowerCase())
                    onActivated: labelField.text = ["", "Red", "Yellow", "Green", "Blue", "Purple"][currentIndex]
                    Accessible.name: qsTranslate("Apertone", "Color label")
                }
            }
            Label { text: qsTranslate("Apertone", "Custom labels are preserved. Standard color names also color the photo card."); Layout.fillWidth: true; wrapMode: Text.Wrap }
            ApertoneButton {
                objectName: "metadata.save"
                text: qsTranslate("Apertone", "Save metadata to catalog"); enabled: dialog.ready; Layout.fillWidth: true
                onClicked: library.saveDescriptiveMetadata({title: titleField.text, caption: captionField.text, creators: creatorsField.text.split("\n"),
                    copyright: copyrightField.text, location: locationField.text, keywords: keywordsField.text.split("\n"),
                    rating: ratingField.currentIndex, flag: flagField.currentIndex - 1, label: labelField.text})
            }
            RowLayout {
                Layout.fillWidth: true
                ApertoneButton { objectName: "metadata.read"; text: qsTranslate("Apertone", "Read original / XMP"); enabled: dialog.ready; onClicked: confirmImport.open(); Layout.fillWidth: true }
                ApertoneButton { objectName: "metadata.write"; text: qsTranslate("Apertone", "Write saved metadata to XMP"); enabled: dialog.ready; onClicked: library.writeMetadataSidecar(); Layout.fillWidth: true }
            }
            ApertoneButton { text: qsTranslate("Apertone", "Reload catalog values"); enabled: dialog.ready; onClicked: library.loadMetadata(library.descriptiveMetadata.id); Layout.fillWidth: true }
            Label { text: qsTranslate("Apertone", "Missing original"); font.bold: true; Layout.topMargin: 10 }
            RowLayout {
                Layout.fillWidth: true
                ApertoneButton { objectName: "metadata.locateOriginal"; text: qsTranslate("Apertone", "Locate original"); enabled: dialog.ready; onClicked: locateFile.open(); Layout.fillWidth: true }
                ApertoneButton { objectName: "metadata.locateFolder"; text: qsTranslate("Apertone", "Locate folder"); enabled: dialog.ready; onClicked: locateFolder.open(); Layout.fillWidth: true }
            }
            CheckBox { id: nearby; objectName: "metadata.locateNearby"; text: qsTranslate("Apertone", "Also find nearby missing files by name and checksum"); checked: true; Layout.fillWidth: true }
        }
    }
}
