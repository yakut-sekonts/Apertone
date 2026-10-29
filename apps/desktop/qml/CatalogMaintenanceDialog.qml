import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: dialog
    objectName: "catalogMaintenance"
    title: qsTranslate("Apertone", "Catalog and backups")
    modal: true; anchors.centerIn: parent
    width: Math.min(parent.width - 40, 560)
    height: Math.min(parent.height - 40, 690)
    standardButtons: Dialog.Close
    property url restoreSource: ""
    readonly property bool maintenanceReady: !library.working && !library.loading &&
        !editor.busy && !editor.saving && !editor.exporting && !editor.aiBusy &&
        !editor.retouchBusy && !editor.creativeBusy && !editor.whiteBalanceBusy &&
        !editor.transformAutoBusy && !editor.editingLocked
    Dialog {
        id: refreshImportDialog
        objectName: "catalog.refreshLastImportDialog"
        title: qsTranslate("Apertone", "Refresh last import?")
        modal: true; anchors.centerIn: parent
        width: Math.min(dialog.width - 40, 480)
        implicitHeight: Math.min(dialog.height - 40, 300)
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            standardButton(Dialog.Ok).objectName = "catalog.refreshLastImportConfirm"
            standardButton(Dialog.Cancel).objectName = "catalog.refreshLastImportCancel"
        }
        contentItem: ScrollView {
            id: refreshImportWarning
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: refreshImportWarning.availableWidth
                Label {
                    text: qsTranslate("Apertone", "Scans the files and folders selected for the last completed import using the same Add, Copy, or Move settings. Existing photos and edits are preserved; new supported photos are imported.")
                    wrapMode: Text.Wrap; Layout.fillWidth: true
                }
                Label {
                    text: qsTranslate("Apertone", "If the previous import used Move, newly found originals are moved again after verification.")
                    wrapMode: Text.Wrap; Layout.fillWidth: true; color: Theme.muted
                }
            }
        }
        onAccepted: library.refreshLastImport()
    }
    Dialog {
        id: clearLibraryDialog
        objectName: "catalog.clearLibraryDialog"
        title: qsTranslate("Apertone", "Clear library?")
        modal: true; anchors.centerIn: parent
        width: Math.min(dialog.width - 40, 480)
        implicitHeight: Math.min(dialog.height - 40, 260)
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            standardButton(Dialog.Ok).objectName = "catalog.clearLibraryConfirm"
            standardButton(Dialog.Cancel).objectName = "catalog.clearLibraryCancel"
        }
        contentItem: ScrollView {
            id: clearLibraryWarning
            clip: true
            contentWidth: availableWidth
            Label {
                width: clearLibraryWarning.availableWidth
                text: qsTranslate("Apertone", "Remove all photos, edits, metadata, import history and local analysis from this catalog? Collections are kept empty. Originals, exported files and backups stay on disk. This cannot be undone.")
                wrapMode: Text.Wrap
            }
        }
        onAccepted: {
            if (editor.closePhoto()) {
                editor.clearPreviewCache()
                library.clearLibrary()
            }
        }
    }
    FileDialog {
        id: newCatalog
        title: qsTranslate("Apertone", "Create catalog")
        fileMode: FileDialog.SaveFile; defaultSuffix: "sqlite3"
        onAccepted: library.openCatalog(selectedFile, true)
    }
    FileDialog {
        id: openCatalog
        title: qsTranslate("Apertone", "Open catalog in a new window")
        nameFilters: ["Apertone catalog (*.sqlite3 *.sqlite *.db)"]
        onAccepted: library.openCatalog(selectedFile, false)
    }
    FileDialog {
        id: backupFile
        title: qsTranslate("Apertone", "Choose catalog backup")
        nameFilters: ["Apertone catalog (*.sqlite3 *.sqlite *.db)"]
        onAccepted: { dialog.restoreSource = selectedFile; restoredFile.open() }
    }
    FileDialog {
        id: restoredFile
        objectName: "catalog.restoreDestination"
        title: qsTranslate("Apertone", "Restore to a new catalog file")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "sqlite3"
        onAccepted: library.restoreCatalog(dialog.restoreSource, selectedFile)
    }
    contentItem: ScrollView {
      clip: true; contentWidth: availableWidth
      ColumnLayout {
        width: parent.width; spacing: 10
        Label { text: editor.catalogPath; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            ApertoneButton { text: qsTranslate("Apertone", "Create catalog"); enabled: !library.working; onClicked: newCatalog.open(); Layout.fillWidth: true }
            ApertoneButton { text: qsTranslate("Apertone", "Open catalog in a new window"); enabled: !library.working; onClicked: openCatalog.open(); Layout.fillWidth: true }
        }
        Label { text: qsTranslate("Apertone", "Library maintenance") }
        ApertoneButton {
            objectName: "catalog.refreshLastImport"
            text: qsTranslate("Apertone", "Refresh last import")
            enabled: dialog.maintenanceReady
            onClicked: refreshImportDialog.open()
            Layout.fillWidth: true
        }
        Label {
            text: qsTranslate("Apertone", "Imports new supported files from the last selected sources. Duplicate photos and existing edits are preserved; changed originals are reported for review.")
            wrapMode: Text.Wrap; color: Theme.muted; Layout.fillWidth: true
        }
        ApertoneButton {
            objectName: "catalog.clearLibrary"
            text: qsTranslate("Apertone", "Clear library")
            enabled: dialog.maintenanceReady
            onClicked: clearLibraryDialog.open()
            Layout.fillWidth: true
        }
        Label { text: qsTranslate("Apertone", "Automatic catalog backup") }
        ApertoneComboBox {
            model: [qsTranslate("Apertone", "Manually"), qsTranslate("Apertone", "Every launch"), qsTranslate("Apertone", "Daily"), qsTranslate("Apertone", "Weekly")]
            currentIndex: library.backupSchedule; onActivated: library.backupSchedule = currentIndex
            Layout.fillWidth: true; Accessible.name: qsTranslate("Apertone", "Automatic catalog backup")
        }
        Label { text: qsTranslate("Apertone", "Backups to keep") }
        ApertoneSpinBox { from: 1; to: 50; value: library.backupKeep; onValueModified: library.backupKeep = value; Accessible.name: qsTranslate("Apertone", "Backups to keep") }
        Label { text: qsTranslate("Apertone", "Catalog backups contain edits and metadata. Keep a separate backup of your originals and generated DNG files."); wrapMode: Text.Wrap; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            ApertoneButton { objectName: "catalog.backupNow"; text: qsTranslate("Apertone", "Back up now"); enabled: !library.working && !editor.saving; onClicked: { editor.endEdit(); library.backupNow() } Layout.fillWidth: true }
            ApertoneButton { text: qsTranslate("Apertone", "Show backups"); onClicked: Qt.openUrlExternally(library.backupFolder); Layout.fillWidth: true }
        }
        ApertoneButton { text: qsTranslate("Apertone", "Restore backup to a new file"); enabled: !library.working; onClicked: backupFile.open(); Layout.fillWidth: true }
        CheckBox { text: qsTranslate("Apertone", "Automatically write metadata to XMP sidecar"); checked: library.autoWriteXMP; enabled: !library.working; onToggled: library.autoWriteXMP = checked; Layout.fillWidth: true }
        ApertoneButton { text: qsTranslate("Apertone", "Write pending XMP sidecars"); enabled: !library.working; onClicked: library.syncSidecars(); Layout.fillWidth: true }
        BusyIndicator { running: library.working; visible: running; Layout.alignment: Qt.AlignHCenter }
        Label { objectName: "catalog.status"; text: library.maintenanceStatus || library.status; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
        Label { objectName: "catalog.notice"; text: library.maintenanceStatus && library.status !== library.maintenanceStatus ? library.status : ""; visible: text.length > 0; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
        Label { objectName: "catalog.error"; text: qsTranslate("Apertone", library.error); textFormat: Text.PlainText; visible: text.length > 0; color: Theme.negative; wrapMode: Text.Wrap; Layout.fillWidth: true }
      }
    }
}
