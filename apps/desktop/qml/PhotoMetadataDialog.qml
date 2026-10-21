import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: dialog
    title: qsTranslate("Luma", "Photo metadata")
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
            copyrightField.text = values.copyright || ""; creatorField.text = values.creator || ""
            locationField.text = values.location || ""; keywordsField.text = (values.keywords || []).join("\n")
            ratingField.currentIndex = values.rating || 0
            labelField.currentIndex = Math.max(0, ["", "red", "yellow", "green", "blue", "purple"].indexOf(values.label || ""))
        }
    }
    FileDialog { id: locateFile; title: qsTranslate("Luma", "Locate original"); onAccepted: library.locateOriginal(selectedFile) }
    FolderDialog { id: locateFolder; title: qsTranslate("Luma", "Locate folder"); onAccepted: library.locateFolder(selectedFolder, nearby.checked) }
    Dialog {
        id: confirmImport
        title: qsTranslate("Luma", "Read metadata from original and XMP?")
        modal: true; anchors.centerIn: parent; width: Math.min(dialog.width - 40, 480)
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            width: parent.width; wrapMode: Text.Wrap
            text: qsTranslate("Luma", "Replaces the catalog metadata and this draft with the values from the original and sidecar. Develop edits remain unchanged.")
        }
        onAccepted: library.importDescriptiveMetadata()
    }
    contentItem: ScrollView {
        clip: true; contentWidth: availableWidth
        ColumnLayout {
            width: parent.width; spacing: 8
            Label { text: library.descriptiveMetadata.path || ""; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
            Label { text: library.descriptiveMetadata.readError || ""; textFormat: Text.PlainText; visible: text.length > 0; color: "#ffc1bc"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: qsTranslate("Luma", library.error); textFormat: Text.PlainText; visible: text.length > 0; color: "#ffc1bc"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: qsTranslate("Luma", "Title") }
            TextField { id: titleField; enabled: dialog.ready; Layout.fillWidth: true; maximumLength: 1024; selectByMouse: true; Accessible.name: qsTranslate("Luma", "Title") }
            Label { text: qsTranslate("Luma", "Caption") }
            TextArea { id: captionField; enabled: dialog.ready; Layout.fillWidth: true; Layout.minimumHeight: 75; wrapMode: Text.Wrap; selectByMouse: true; Accessible.name: qsTranslate("Luma", "Caption") }
            Label { text: qsTranslate("Luma", "Creator") }
            TextField { id: creatorField; enabled: dialog.ready; Layout.fillWidth: true; maximumLength: 1024; selectByMouse: true; Accessible.name: qsTranslate("Luma", "Creator") }
            Label { text: qsTranslate("Luma", "Copyright") }
            TextField { id: copyrightField; enabled: dialog.ready; Layout.fillWidth: true; maximumLength: 2048; selectByMouse: true; Accessible.name: qsTranslate("Luma", "Copyright") }
            Label { text: qsTranslate("Luma", "Location") }
            TextField { id: locationField; enabled: dialog.ready; Layout.fillWidth: true; maximumLength: 2048; selectByMouse: true; Accessible.name: qsTranslate("Luma", "Location") }
            Label { text: qsTranslate("Luma", "Keywords · one per line") }
            TextArea { id: keywordsField; enabled: dialog.ready; Layout.fillWidth: true; Layout.minimumHeight: 75; wrapMode: Text.Wrap; selectByMouse: true; Accessible.name: qsTranslate("Luma", "Keywords · one per line") }
            RowLayout {
                Layout.fillWidth: true
                ComboBox { id: ratingField; enabled: dialog.ready; model: ["0 ★", "1 ★", "2 ★", "3 ★", "4 ★", "5 ★"]; Accessible.name: qsTranslate("Luma", "Rating"); Layout.fillWidth: true }
                ComboBox { id: labelField; enabled: dialog.ready; model: [qsTranslate("Luma", "No label"), qsTranslate("Luma", "Red"), qsTranslate("Luma", "Yellow"), qsTranslate("Luma", "Green"), qsTranslate("Luma", "Blue"), qsTranslate("Luma", "Purple")]; Accessible.name: qsTranslate("Luma", "Color label"); Layout.fillWidth: true }
            }
            Button {
                text: qsTranslate("Luma", "Save metadata to catalog"); enabled: dialog.ready; Layout.fillWidth: true
                onClicked: library.saveDescriptiveMetadata({title: titleField.text, caption: captionField.text, creator: creatorField.text,
                    copyright: copyrightField.text, location: locationField.text, keywords: keywordsField.text.split("\n"),
                    rating: ratingField.currentIndex, label: ["", "red", "yellow", "green", "blue", "purple"][labelField.currentIndex]})
            }
            RowLayout {
                Layout.fillWidth: true
                Button { text: qsTranslate("Luma", "Read original / XMP"); enabled: dialog.ready; onClicked: confirmImport.open(); Layout.fillWidth: true }
                Button { text: qsTranslate("Luma", "Write saved metadata to XMP"); enabled: dialog.ready; onClicked: library.writeMetadataSidecar(); Layout.fillWidth: true }
            }
            Button { text: qsTranslate("Luma", "Reload catalog values"); enabled: dialog.ready; onClicked: library.loadMetadata(library.descriptiveMetadata.id); Layout.fillWidth: true }
            Label { text: qsTranslate("Luma", "Missing original"); font.bold: true; Layout.topMargin: 10 }
            RowLayout {
                Layout.fillWidth: true
                Button { text: qsTranslate("Luma", "Locate original"); enabled: dialog.ready; onClicked: locateFile.open(); Layout.fillWidth: true }
                Button { text: qsTranslate("Luma", "Locate folder"); enabled: dialog.ready; onClicked: locateFolder.open(); Layout.fillWidth: true }
            }
            CheckBox { id: nearby; text: qsTranslate("Luma", "Also find nearby missing files by name and checksum"); checked: true; Layout.fillWidth: true }
        }
    }
}
