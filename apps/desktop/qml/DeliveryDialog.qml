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
    property bool webdav: format.currentIndex === 4
    property bool published: false
    property bool printed: false
    readonly property bool validFolderName: {
        const name = folderName.text.trim()
        return name.length > 0 && !name.startsWith(".") && !name.endsWith(".") &&
                !name.split("").some(ch => ch.charCodeAt(0) < 32 || '\\/:*?"<>|'.includes(ch))
    }
    onAboutToShow: { completedFile = ""; published = false; printed = false; updateGallery.checked = false }
    onClosed: password.clear()
    Connections {
        target: library
        function onDeliveryReady(url) {
            dialog.completedFile = url
            dialog.published = !url.toString()
        }
        function onPrintReady(url) {
            dialog.completedFile = url
            dialog.printed = true
        }
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
                    qsTranslate("Apertone", "HTML gallery"), qsTranslate("Apertone", "HTML slideshow"),
                    qsTranslate("Apertone", "WebDAV gallery")]
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
            Accessible.name: dialog.webdav && updateGallery.checked ? qsTranslate("Apertone", "Published gallery folder")
                                                                     : qsTranslate("Apertone", "New folder name")
        }
        RowLayout {
            visible: dialog.pdf
            ApertoneButton { text: qsTranslate("Apertone", "Choose proof ICC…"); onClicked: proofChooser.open() }
            Label { text: dialog.profile.toString().split("/").pop(); elide: Text.ElideMiddle; Layout.fillWidth: true }
            ApertoneButton { visible: !!dialog.profile.toString(); text: qsTranslate("Apertone", "Clear"); onClicked: dialog.profile = "" }
        }
        ApertoneTextField {
            id: endpoint
            objectName: "delivery.webDavEndpoint"
            visible: dialog.webdav
            enabled: !library.preparingDelivery
            Layout.fillWidth: true; maximumLength: 2048
            placeholderText: qsTranslate("Apertone", "HTTPS WebDAV folder URL")
            Accessible.name: placeholderText
        }
        RowLayout {
            visible: dialog.webdav
            Layout.fillWidth: true
            ApertoneTextField {
                id: username
                Layout.fillWidth: true; maximumLength: 512
                enabled: !library.preparingDelivery
                placeholderText: qsTranslate("Apertone", "WebDAV username")
                Accessible.name: placeholderText
            }
            ApertoneTextField {
                id: password
                Layout.fillWidth: true; maximumLength: 4096
                enabled: !library.preparingDelivery
                echoMode: TextInput.Password
                placeholderText: qsTranslate("Apertone", "WebDAV password")
                Accessible.name: placeholderText
            }
        }
        ApertoneCheckBox {
            id: updateGallery
            objectName: "delivery.webDavUpdate"
            visible: dialog.webdav
            enabled: !library.preparingDelivery
            text: qsTranslate("Apertone", "Update a gallery published by this catalog")
        }
        ProgressBar { visible: library.preparingDelivery; value: library.progress; Layout.fillWidth: true }
        Label {
            visible: dialog.webdav && dialog.published && !library.preparingDelivery
            text: qsTranslate("Apertone", "Gallery published.")
            Layout.fillWidth: true
        }
        Label {
            visible: dialog.pdf && dialog.printed && !library.preparingDelivery
            text: dialog.completedFile.toString() ? qsTranslate("Apertone", "PDF saved.")
                                                  : qsTranslate("Apertone", "Print job submitted.")
            Layout.fillWidth: true
        }
        Label {
            visible: !!library.error
            text: qsTranslate("Apertone", library.error)
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
            objectName: "delivery.print"
            visible: dialog.pdf
            enabled: !library.working && !!heading.text.trim()
            text: qsTranslate("Apertone", "Print…")
            onClicked: {
                dialog.completedFile = ""
                dialog.printed = false
                library.printSelection(format.currentIndex, heading.text, dialog.profile)
            }
        }
        ApertoneButton {
            objectName: "delivery.create"
            text: dialog.webdav ? (updateGallery.checked ? qsTranslate("Apertone", "Update gallery")
                                                         : qsTranslate("Apertone", "Publish gallery"))
                                : qsTranslate("Apertone", "Create")
            enabled: !library.working && !!heading.text.trim() && (dialog.pdf || dialog.validFolderName)
                     && (!dialog.webdav || !!endpoint.text.trim())
            onClicked: {
                dialog.printed = false
                if (dialog.webdav) {
                    dialog.completedFile = ""
                    dialog.published = false
                    library.publishWebDav(heading.text, endpoint.text, folderName.text, username.text,
                                          password.text, updateGallery.checked)
                    password.clear()
                } else if (dialog.pdf) {
                    pdfChooser.open()
                } else {
                    galleryChooser.open()
                }
            }
        }
    }
}
