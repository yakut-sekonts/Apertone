import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    objectName: "library.collectionSetDialog"
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    title: qsTranslate("Apertone", "Collection sets")
    width: Math.min(460, parent.width - 40)
    standardButtons: Dialog.Close

    required property var choices
    property string selectedID: ""
    property bool saving: false
    readonly property var editableSets: [{id: "", path: qsTranslate("Apertone", "New collection set")}].concat(library.collectionSets)

    function edit(id) {
        open()
        select(id)
    }

    function select(id) {
        const item = library.collectionSets.find(row => row.id === id)
        selectedID = item ? item.id : ""
        setName.text = item ? item.name : ""
        setParent.currentIndex = Math.max(0, setParent.indexOfValue(item ? item.parent : ""))
        setName.forceActiveFocus()
    }

    ColumnLayout {
        width: parent.width
        ApertoneComboBox {
            id: setList
            objectName: "library.collectionSetChoice"
            Layout.fillWidth: true; Layout.minimumWidth: 0
            model: dialog.editableSets; textRole: "path"; valueRole: "id"
            currentIndex: Math.max(0, indexOfValue(dialog.selectedID))
            Accessible.name: qsTranslate("Apertone", "Collection set")
            enabled: !library.working
            onActivated: dialog.select(currentValue || "")
        }
        ApertoneTextField {
            id: setName
            objectName: "library.collectionSetName"
            Layout.fillWidth: true; maximumLength: 128
            placeholderText: qsTranslate("Apertone", "Set name")
            enabled: !library.working
        }
        Label { text: qsTranslate("Apertone", "Parent set") }
        ApertoneComboBox {
            id: setParent
            objectName: "library.collectionSetParent"
            Layout.fillWidth: true; Layout.minimumWidth: 0
            model: dialog.choices.filter(row => !dialog.selectedID || row.id !== dialog.selectedID)
            textRole: "path"; valueRole: "id"
            Accessible.name: qsTranslate("Apertone", "Parent set")
            enabled: !library.working
        }
        RowLayout {
            Layout.fillWidth: true
            ApertoneButton {
                objectName: "library.collectionSetSave"
                text: qsTranslate("Apertone", "Save")
                enabled: !library.working && setName.text.trim().length > 0
                onClicked: {
                    dialog.saving = true
                    library.saveCollectionSet(dialog.selectedID, setName.text, setParent.currentValue || "")
                }
            }
            ApertoneButton {
                objectName: "library.collectionSetDelete"
                text: qsTranslate("Apertone", "Delete empty set")
                enabled: !library.working && dialog.selectedID.length > 0
                onClicked: {
                    dialog.saving = true
                    library.deleteCollectionSet(dialog.selectedID)
                }
            }
        }
        Label {
            visible: library.error.length > 0
            text: qsTranslate("Apertone", library.error)
            color: Theme.negative; wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    Connections {
        target: library
        function onStateChanged() {
            if (dialog.saving && !library.working && !library.loading) {
                dialog.saving = false
                if (!library.error.length) dialog.close()
            }
        }
    }
    onClosed: saving = false
}
