import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: dialog
    title: qsTranslate("Apertone", "Diagnostics")
    modal: true; anchors.centerIn: parent
    width: Math.min(parent.width - 40, 600)
    height: Math.min(parent.height - 40, 680)
    standardButtons: Dialog.Close
    onOpened: diagnostics.refresh()
    function mib(bytes) { return bytes > 0 ? (bytes / 1048576).toFixed(1) + " MiB" : "—" }
    function cacheText(value) {
        return value ? (value.usedBytes / 1048576).toFixed(1) + " / " +
            (value.budgetBytes / 1048576).toFixed(0) + " MiB · " + value.entries : "—"
    }
    FileDialog {
        id: destination
        title: qsTranslate("Apertone", "Export Diagnostic Report")
        fileMode: FileDialog.SaveFile; defaultSuffix: "json"
        nameFilters: ["JSON (*.json)"]
        onAccepted: diagnostics.exportReport(selectedFile)
    }
    contentItem: ScrollView {
        clip: true; contentWidth: availableWidth
        ColumnLayout {
            width: parent.width; spacing: 10
            Label { text: "Apertone " + Qt.application.version; font.pixelSize: 20 }
            Label { text: editor.catalogPath; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
            Label {
                text: qsTranslate("Apertone", "Catalog schema") + ": " + (diagnostics.snapshot.catalogSchema || "—")
                Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            Label { text: (diagnostics.snapshot.os || "—") + " · " + (diagnostics.snapshot.architecture || "—"); wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: "CPU: " + (diagnostics.snapshot.cpu || "—") + " · " + (diagnostics.snapshot.logicalCores || "—"); wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: "GPU: " + (diagnostics.snapshot.gpu || "—"); wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label {
                text: diagnostics.snapshot.metalPreviewRequested && diagnostics.snapshot.metalAvailable
                    ? qsTranslate("Apertone", "Preview policy: Metal with CPU fallback") : qsTranslate("Apertone", "Preview policy: CPU")
                wrapMode: Text.Wrap; Layout.fillWidth: true
            }
            Label { text: "RAM: " + dialog.mib(diagnostics.snapshot.physicalMemoryBytes) + " · RSS: " + dialog.mib(diagnostics.snapshot.residentMemoryBytes); wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: qsTranslate("Apertone", "Decoded preview cache") + ": " + dialog.cacheText(diagnostics.snapshot.decodedPreviewCache); wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: qsTranslate("Apertone", "Full-resolution cache") + ": " + dialog.cacheText(diagnostics.snapshot.fullResolutionCache); wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: qsTranslate("Apertone", "Rendered preview cache") + ": " + dialog.cacheText(diagnostics.snapshot.renderedPreviewCache); wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: "Qt " + (diagnostics.snapshot.qtVersion || "—") + " · SQLite " + (diagnostics.snapshot.sqliteVersion || "—"); wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: qsTranslate("Apertone", "Cache sizes count retained entries; active operations can use additional memory."); wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: qsTranslate("Apertone", "The report contains technical details only. Photos, filenames, catalog paths, metadata, model paths and credentials are excluded."); wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label {
                visible: !!diagnostics.snapshot.localDiagnostics
                text: diagnostics.snapshot.localDiagnostics && diagnostics.snapshot.localDiagnostics.crashCaptureEnabled
                    ? qsTranslate("Apertone", "Local crash recording is enabled.") : qsTranslate("Apertone", "Local crash recording is unavailable.")
                wrapMode: Text.Wrap; Layout.fillWidth: true
            }
            Label {
                visible: !!diagnostics.snapshot.localDiagnostics
                text: diagnostics.snapshot.localDiagnostics && diagnostics.snapshot.localDiagnostics.loggingAvailable
                    ? qsTranslate("Apertone", "Local event logging is enabled.") : qsTranslate("Apertone", "Local event logging is unavailable.")
                wrapMode: Text.Wrap; Layout.fillWidth: true
            }
            Label {
                visible: !!diagnostics.snapshot.localDiagnostics && diagnostics.snapshot.localDiagnostics.droppedLogEvents > 0
                text: qsTranslate("Apertone", "Unwritten log events: %1").arg(diagnostics.snapshot.localDiagnostics ? diagnostics.snapshot.localDiagnostics.droppedLogEvents : 0)
                wrapMode: Text.Wrap; Layout.fillWidth: true
            }
            ApertoneButton { text: qsTranslate("Apertone", "Open local logs and crash reports"); enabled: diagnostics.localFolder.toString().length > 0; onClicked: Qt.openUrlExternally(diagnostics.localFolder); Layout.fillWidth: true }
            Label { text: qsTranslate("Apertone", "Logs and crash reports stay on this Mac and are not included in the diagnostic JSON."); wrapMode: Text.Wrap; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                ApertoneButton { text: qsTranslate("Apertone", "Refresh"); enabled: !diagnostics.busy; onClicked: diagnostics.refresh(); Layout.fillWidth: true }
                ApertoneButton { text: qsTranslate("Apertone", "Export Diagnostic Report"); enabled: !diagnostics.busy; onClicked: destination.open(); Layout.fillWidth: true }
            }
            BusyIndicator { running: diagnostics.busy; visible: running; Layout.alignment: Qt.AlignHCenter }
            Label { text: qsTranslate("Apertone", diagnostics.error); textFormat: Text.PlainText; visible: text.length > 0; color: Theme.negative; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
            Label { text: qsTranslate("Apertone", "Report saved: %1").arg(diagnostics.savedPath); textFormat: Text.PlainText; visible: diagnostics.savedPath.length > 0; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
        }
    }
}
