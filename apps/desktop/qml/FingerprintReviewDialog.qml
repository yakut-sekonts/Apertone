import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: fingerprintDialog; objectName: "library.fingerprintDialog"
    title: qsTranslate("Apertone", "Catalog fingerprint matches")
    required property var libraryModel
    modal: true; anchors.centerIn: parent
    width: Math.min(parent.width - 40, 720); height: Math.min(parent.height - 40, 500)
    standardButtons: Dialog.Close
    onOpened: libraryModel.openFingerprintReview()
    onClosed: libraryModel.closeFingerprintReview()
    ColumnLayout {
        anchors.fill: parent; spacing: Theme.panelSpacing
        Label {
            objectName: "library.fingerprintExplanation"
            text: qsTranslate("Apertone", "Matches use the SHA-256 and size saved at import. Originals may have changed or become unavailable; no files are checked or changed here.")
            wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        Label { text: qsTranslate("Apertone", "Results may change while the catalog is edited. Refresh to start again."); wrapMode: Text.Wrap; Layout.fillWidth: true }
        Label { visible: libraryModel.fingerprintBusy; text: qsTranslate("Apertone", "Loading…"); Layout.fillWidth: true }
        Label { objectName: "library.fingerprintError"; visible: libraryModel.fingerprintError.length > 0; text: qsTranslate("Apertone", libraryModel.fingerprintError); textFormat: Text.PlainText; color: Theme.negative; wrapMode: Text.Wrap; Layout.fillWidth: true }
        Label { objectName: "library.fingerprintEmpty"; visible: !libraryModel.fingerprintBusy && libraryModel.fingerprintError.length === 0 && libraryModel.fingerprintRows.length === 0; text: qsTranslate("Apertone", "No matching saved fingerprints."); Layout.fillWidth: true }
        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
            Column {
                width: parent.width; spacing: Theme.panelSpacing
                Repeater {
                    model: libraryModel.fingerprintRows
                    Rectangle {
                        required property var modelData
                        width: parent.width; implicitHeight: fingerprintText.implicitHeight + 16
                        color: Theme.control; radius: Theme.radius
                        TextEdit {
                            id: fingerprintText; objectName: "library.fingerprintRow"
                            anchors.fill: parent; anchors.margins: 8; readOnly: true; selectByMouse: true
                            text: modelData.checksum + " · " + modelData.size + " B\n" + modelData.assetID + "\n" + modelData.path
                            color: Theme.text; font.pixelSize: 11; wrapMode: TextEdit.WrapAnywhere; textFormat: TextEdit.PlainText
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            ApertoneButton { objectName: "library.fingerprintRefresh"; text: qsTranslate("Apertone", "Refresh"); enabled: !libraryModel.fingerprintBusy && !libraryModel.working; onClicked: libraryModel.openFingerprintReview() }
            Item { Layout.fillWidth: true }
            ApertoneButton { objectName: "library.fingerprintPrevious"; text: qsTranslate("Apertone", "Previous"); enabled: !libraryModel.fingerprintBusy && libraryModel.fingerprintHasPrevious; onClicked: libraryModel.previousFingerprintPage() }
            ApertoneButton { objectName: "library.fingerprintNext"; text: qsTranslate("Apertone", "Next"); enabled: !libraryModel.fingerprintBusy && libraryModel.fingerprintHasNext; onClicked: libraryModel.nextFingerprintPage() }
        }
    }
}
