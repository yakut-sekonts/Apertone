import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: dialog
    objectName: "library.deliveryDialog"
    title: qsTranslate("Apertone", "Deliver selected photos")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(510, parent.width - 32)
    modal: true
    property url profile
    property url completedFile
    property bool pdf: format.currentIndex < 2
    onAboutToShow: completedFile = ""
    Connections {
        target: library
        function onDeliveryReady(url) { dialog.completedFile = url }
    }
    FileDialog {
        id: proofChooser
        title: qsTranslate("Apertone", "Proof ICC profile")
        nameFilters: ["ICC (*.icc *.icm *.ICC *.ICM)"]
        onAccepted: dialog.profile = selectedFile
    }
    FileDialog {
        id: pdfChooser
        title: qsTranslate("Apertone", "Save PDF")
        fileMode: FileDialog.SaveFile
        nameFilters: ["PDF (*.pdf)"]
        defaultSuffix: "pdf"
        onAccepted: library.deliver(format.currentIndex, heading.text, selectedFile, dialog.profile)
    }
    FolderDialog {
        id: galleryChooser
        title: qsTranslate("Apertone", "Choose a parent folder")
        onAccepted: {
            const folder = selectedFolder.toString().replace(/\/$/, "")
            library.deliver(format.currentIndex, heading.text,
                            folder + "/" + encodeURIComponent(folderName.text.trim()), "")
        }
    }
    contentItem: ColumnLayout {
        ApertoneComboBox {
            id: format
            objectName: "delivery.format"
            Layout.fillWidth: true
            enabled: !library.preparingDelivery
            model: [qsTranslate("Apertone", "Contact sheet PDF"), qsTranslate("Apertone", "Photo book PDF"),
                    qsTranslate("Apertone", "HTML gallery"), qsTranslate("Apertone", "HTML slideshow")]
        }
        ApertoneTextField {
            id: heading
            Layout.fillWidth: true
            text: qsTranslate("Apertone", "Selected photos")
            maximumLength: 160
            Accessible.name: qsTranslate("Apertone", "Title")
        }
        ApertoneTextField {
            id: folderName
            visible: !dialog.pdf
            Layout.fillWidth: true
            text: "Apertone-gallery"
            maximumLength: 80
            validator: RegularExpressionValidator { regularExpression: /[^.\\/:*?"<>|\u0000-\u001f][^\\/:*?"<>|\u0000-\u001f]{0,79}/ }
            Accessible.name: qsTranslate("Apertone", "New folder name")
        }
        RowLayout {
            visible: dialog.pdf
            ApertoneButton { text: qsTranslate("Apertone", "Choose proof ICC…"); onClicked: proofChooser.open() }
            Label { text: dialog.profile.toString().split("/").pop(); elide: Text.ElideMiddle; Layout.fillWidth: true }
            ApertoneButton { visible: !!dialog.profile.toString(); text: qsTranslate("Apertone", "Clear"); onClicked: dialog.profile = "" }
        }
        ProgressBar { visible: library.preparingDelivery; value: library.progress; Layout.fillWidth: true }
        Label {
            visible: !!library.error
            text: library.error
            textFormat: Text.PlainText
            color: Theme.negative
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }
    footer: DialogButtonBox {
        ApertoneButton {
            text: library.preparingDelivery ? qsTranslate("Apertone", "Cancel") : qsTranslate("Apertone", "Close")
            onClicked: library.preparingDelivery ? library.cancelDelivery() : dialog.close()
        }
        ApertoneButton {
            visible: !!dialog.completedFile.toString()
            text: qsTranslate("Apertone", "Open output")
            onClicked: Qt.openUrlExternally(dialog.completedFile)
        }
        ApertoneButton {
            objectName: "delivery.create"
            text: qsTranslate("Apertone", "Create")
            enabled: !library.working && !!heading.text.trim() && (dialog.pdf || folderName.acceptableInput)
            onClicked: dialog.pdf ? pdfChooser.open() : galleryChooser.open()
        }
    }
}
