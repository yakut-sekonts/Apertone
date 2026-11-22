import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    objectName: "library.keywordDialog"
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    title: qsTranslate("Apertone", "Keyword hierarchy")
    width: Math.min(560, parent.width - 40)
    standardButtons: Dialog.Close

    property string selectedPath: ""
    property string pendingPath: ""
    property bool saving: false
    readonly property var choices: [{path: "", label: qsTranslate("Apertone", "New keyword"), aliases: []}]
                                   .concat(library.catalogKeywords.map(row => ({path: row.path,
                                      label: row.path.replaceAll("|", " › ") + " · " + row.count,
                                      aliases: row.aliases})))

    function select(path) {
        const term = choices.find(row => row.path === path)
        selectedPath = term ? term.path : ""
        keywordPath.text = selectedPath
        aliases.text = term ? term.aliases.join("\n") : ""
    }

    onOpened: { select(""); library.loadCatalogKeywords() }
    onClosed: saving = false

    ColumnLayout {
        width: parent.width
        ApertoneComboBox {
            id: tree
            objectName: "library.keywordChoice"
            Layout.fillWidth: true; Layout.minimumWidth: 0
            model: dialog.choices; textRole: "label"; valueRole: "path"
            currentIndex: Math.max(0, indexOfValue(dialog.selectedPath))
            Accessible.name: qsTranslate("Apertone", "Keyword hierarchy")
            enabled: !library.working
            onActivated: dialog.select(currentValue || "")
        }
        ApertoneTextField {
            id: keywordPath
            objectName: "library.keywordPath"
            Layout.fillWidth: true; maximumLength: 512
            placeholderText: qsTranslate("Apertone", "Parent|Child")
            Accessible.name: qsTranslate("Apertone", "Keyword path")
            enabled: !library.working
        }
        Label { text: qsTranslate("Apertone", "Aliases, one per line") }
        ScrollView {
            Layout.fillWidth: true; Layout.preferredHeight: 78
            TextArea {
                id: aliases
                objectName: "library.keywordAliases"
                Accessible.name: qsTranslate("Apertone", "Aliases, one per line")
                color: Theme.text
                selectByMouse: true; enabled: !library.working
            }
        }
        RowLayout {
            Layout.fillWidth: true
            ApertoneButton {
                objectName: "library.keywordSave"
                text: qsTranslate("Apertone", "Save")
                enabled: keywordPath.text.trim().length > 0 && !library.working
                onClicked: {
                    dialog.pendingPath = keywordPath.text.trim()
                    dialog.saving = true
                    library.saveCatalogKeyword(dialog.selectedPath, dialog.pendingPath, aliases.text.split("\n"))
                }
            }
            ApertoneButton {
                text: qsTranslate("Apertone", "Delete keyword…")
                enabled: dialog.selectedPath.length > 0 && !library.working
                onClicked: confirmation.open()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            ApertoneButton {
                objectName: "library.keywordAssign"
                text: qsTranslate("Apertone", "Add to selected photos")
                enabled: dialog.selectedPath.length > 0 && library.selectedCount > 0 && !library.working
                onClicked: library.keywordMembership(dialog.selectedPath, true)
            }
            ApertoneButton {
                text: qsTranslate("Apertone", "Remove from selected photos")
                enabled: dialog.selectedPath.length > 0 && library.selectedCount > 0 && !library.working
                onClicked: library.keywordMembership(dialog.selectedPath, false)
            }
        }
        Label {
            visible: library.error.length > 0
            text: qsTranslate("Apertone", library.error)
            color: Theme.negative; wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    Dialog {
        id: confirmation
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true; width: 380
        title: qsTranslate("Apertone", "Delete keyword and its children?")
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            width: parent.width; wrapMode: Text.Wrap
            text: qsTranslate("Apertone", "This removes their assignments from catalog photos.")
        }
        onAccepted: {
            dialog.pendingPath = ""
            dialog.saving = true
            library.deleteCatalogKeyword(dialog.selectedPath)
        }
    }
    Connections {
        target: library
        function onCatalogKeywordsChanged() {
            if (dialog.saving && !library.error.length)
                Qt.callLater(dialog.finishSave)
        }
        function onStateChanged() {
            if (dialog.saving && !library.working && library.error.length > 0)
                dialog.saving = false
        }
    }

    function finishSave() {
        if (!saving || library.error.length)
            return
        select(pendingPath)
        saving = false
    }
}
