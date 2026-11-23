import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Rectangle {
    id: pane
    color: Theme.window
    property string currentCollection: ""
    property string editCollectionID: ""
    property string currentSet: ""
    property bool leftPanelVisible: true
    property bool viewToolbarVisible: true
    property string viewMode: "grid"
    property string queuedView: ""
    property string loupeID: ""
    property int loupeRow: -1
    property int pendingLoupeRow: -1
    property real loupeCenterX: 0.5
    property real loupeCenterY: 0.5
    readonly property bool reviewDialogVisible: compareDialog.visible || surveyDialog.visible
    readonly property bool canOpenInDevelop: library.selectedCount > 0 && !library.working &&
                                             !library.selecting && !editor.busy && !reviewDialogVisible
    readonly property var setChoices: [{id: "", path: qsTranslate("Apertone", "Top level")}].concat(library.collectionSets)
    readonly property var activeCollection: library.collections.find(row => row.id === currentCollection)
    onActiveCollectionChanged: if (activeCollection) currentSet = activeCollection.set
    onSetChoicesChanged: if (!setChoices.some(row => row.id === currentSet)) currentSet = ""
    property string intelligentFaceGroupID: ""
    property var intelligenceGroups: [{"id":"", "displayName":qsTranslate("Apertone", "All face groups"), "name":"", "count":0}].concat(library.faceGroups)
    onIntelligenceGroupsChanged: Qt.callLater(function() {
        const index = intelligenceGroups.findIndex(group => group.id === pane.intelligentFaceGroupID)
        if (index < 0) pane.intelligentFaceGroupID = ""
        faceGroupFilter.currentIndex = Math.max(0, index)
    })
    CompareDialog { id: compareDialog; objectName: "library.compare"; onClosed: pane.reviewClosed() }
    SurveyDialog { id: surveyDialog; onClosed: pane.reviewClosed() }
    DeliveryDialog { id: deliveryDialog }
    PhotoMapDialog { id: photoMapDialog }
    SmartCollectionDialog { id: smartCollection }
    CollectionSetDialog { id: collectionSets; choices: pane.setChoices }
    KeywordDialog { id: keywordEditor }
    SemanticSearchDialog { id: semanticSearch }
    PhotoMetadataDialog { id: photoMetadata }
    Dialog {
        id: faceGroupDialog; objectName: "intelligence.faceGroupDialog"
        title: qsTranslate("Apertone", "Name face group")
        modal: true; anchors.centerIn: parent; width: 360; implicitWidth: 360
        standardButtons: Dialog.Ok | Dialog.Cancel
        ApertoneTextField {
            id: faceGroupName; objectName: "intelligence.faceGroupName"
            width: 300; maximumLength: 128; selectByMouse: true
            placeholderText: qsTranslate("Apertone", "Group name")
        }
        onOpened: {
            const group = pane.intelligenceGroups.find(item => item.id === pane.intelligentFaceGroupID)
            faceGroupName.text = group ? group.name : ""
            faceGroupName.forceActiveFocus()
        }
        onAccepted: {
            if (pane.intelligentFaceGroupID.length > 0)
                library.renameFaceGroup(pane.intelligentFaceGroupID, faceGroupName.text)
        }
    }
    Dialog {
        id: applySuggestionsDialog; objectName: "intelligence.applyDialog"
        title: qsTranslate("Apertone", "Apply suggestions?")
        modal: true; anchors.centerIn: parent; width: 440; implicitWidth: 440
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            width: 380; wrapMode: Text.Wrap
            text: library.selectedCount > 0
                  ? qsTranslate("Apertone", "Only unflagged selected photos will receive Pick, Review or Reject flags.")
                  : qsTranslate("Apertone", "Only unflagged photos visible through the current filters will receive Pick, Review or Reject flags.")
        }
        onAccepted: library.applyCullingSuggestions()
    }
    Dialog {
        id: clearAnalysisDialog; objectName: "intelligence.clearDialog"
        title: qsTranslate("Apertone", "Delete local analysis?")
        modal: true; anchors.centerIn: parent; width: 440; implicitWidth: 440
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            width: 380; wrapMode: Text.Wrap
            text: qsTranslate("Apertone", "Deletes suggestions, similar-face groups and burst groups from this catalog. Existing catalog backups may retain older analysis.")
        }
        onAccepted: { pane.intelligentFaceGroupID = ""; library.clearCullingIndex() }
    }
    Dialog {
        id: panoramaDialog; objectName: "merge.panorama"
        title: qsTranslate("Apertone", "Panorama merge")
        modal: true; anchors.centerIn: parent
        width: Math.min(pane.width - 40, 460)
        standardButtons: Dialog.Cancel
        ColumnLayout {
            width: parent.width
            Label {
                text: qsTranslate("Apertone", "Merge 2–24 overlapping originals in capture order. Creates an auto-cropped Float DNG without current Develop edits.")
                wrapMode: Text.Wrap; Layout.fillWidth: true
            }
            ApertoneComboBox {
                id: panoramaProjection; objectName: "merge.projection"
                model: [qsTranslate("Apertone", "Perspective"), qsTranslate("Apertone", "Cylindrical"), qsTranslate("Apertone", "Spherical")]
                currentIndex: 1; Layout.fillWidth: true
                Accessible.name: qsTranslate("Apertone", "Projection")
            }
            Label { text: qsTranslate("Apertone", "Frame field of view") + " · " + Math.round(panoramaFOV.value) + "°" }
            ApertoneSlider {
                id: panoramaFOV; from: 10; to: 140; value: 60; stepSize: 1
                enabled: panoramaProjection.currentIndex !== 0
                Layout.fillWidth: true; Accessible.name: qsTranslate("Apertone", "Frame field of view")
            }
            ApertoneCheckBox { id: panoramaWarp; text: qsTranslate("Apertone", "Boundary warp · stretch edges to reduce cropping"); Layout.fillWidth: true }
            ApertoneButton {
                objectName: "merge.panorama.create"
                text: qsTranslate("Apertone", "Create panorama")
                enabled: library.selectedCount >= 2 && library.selectedCount <= 24 && !library.working && !editor.aiBusy && !editor.busy
                Layout.fillWidth: true
                onClicked: { editor.mergePanoramaPhotos(library.selectedAssetIDs(), panoramaProjection.currentIndex, panoramaFOV.value, panoramaWarp.checked); panoramaDialog.close() }
            }
        }
    }
    Dialog {
        id: hdrDialog; objectName: "merge.hdr"
        title: qsTranslate("Apertone", "HDR merge")
        modal: true; anchors.centerIn: parent
        width: Math.min(pane.width - 40, 440)
        standardButtons: Dialog.Cancel
        ColumnLayout {
            width: parent.width
            Label {
                text: qsTranslate("Apertone", "Merge 2–9 bracketed originals into a new editable Float DNG. Current Develop edits are not applied.")
                wrapMode: Text.Wrap; Layout.fillWidth: true
            }
            Label { text: qsTranslate("Apertone", "Deghosting") + " · " + Math.round(hdrDeghost.value) + "%" }
            ApertoneCheckBox {
                id: hdrAlignment; objectName: "merge.alignment"
                text: qsTranslate("Apertone", "Align frames"); checked: true
                Layout.fillWidth: true
            }
            ApertoneSlider { id: hdrDeghost; objectName: "merge.deghost"; from: 0; to: 100; value: 50; Layout.fillWidth: true; Accessible.name: qsTranslate("Apertone", "Deghosting") }
            ApertoneButton {
                objectName: "merge.hdr.create"
                text: qsTranslate("Apertone", "Create HDR")
                enabled: library.selectedCount >= 2 && library.selectedCount <= 9 && !library.working && !editor.aiBusy && !editor.busy
                Layout.fillWidth: true
                onClicked: { editor.mergeHDRPhotos(library.selectedAssetIDs(), hdrDeghost.value, hdrAlignment.checked); hdrDialog.close() }
            }
        }
    }
    Connections {
        target: library
        function onReloadStarted(preserveViewport) {
            grid.captureViewport(preserveViewport)
            pane.pendingLoupeRow = -1
            if (pane.viewMode === "loupe" && !preserveViewport)
                pane.requestView("grid")
        }
        function onViewportRestored(row) { grid.restoreViewport(row) }
        function onDataChanged() {
            if (pane.pendingLoupeRow >= 0)
                Qt.callLater(pane.finishLoupeNavigation)
        }
    }
    function applyFilter() { library.filter(search.text, ratingFilter.currentIndex, flagFilter.currentIndex - 2, ["any","","red","yellow","green","blue","purple"][labelFilter.currentIndex], currentCollection) }
    function applyIntelligentFilter() {
        library.setIntelligentFilter([-2, 1, 0, -1][suggestionFilter.currentIndex], pane.intelligentFaceGroupID, bestOfBurst.checked)
    }
    function textInputFocused() {
        const item = Window.window ? Window.window.activeFocusItem : null
        return item && typeof item.cursorPosition === "number"
    }
    function allowShortcut() { return library.selectedCount > 0 && !library.working && !pane.textInputFocused() }
    readonly property bool canSurvey: library.selectedCount >= 2 && library.selectedCount <= 12 &&
                                      !library.working && !library.selecting && !editor.busy
    function canRequestView(view) {
        if (view === "grid")
            return true
        if (library.working || library.loading || library.selecting || editor.busy)
            return false
        if (view === "survey")
            return pane.canSurvey
        if (view === "compare")
            return library.selectedCount >= 2 || (library.selectedCount === 1 && library.count >= 2)
        return view === "loupe" && library.count > 0
    }
    function reviewClosed() {
        pane.viewMode = "grid"
        const next = pane.queuedView
        pane.queuedView = ""
        if (next !== "")
            Qt.callLater(pane.requestView, next)
    }
    function requestView(view) {
        if (!pane.canRequestView(view))
            return false
        if (pane.reviewDialogVisible) {
            if (view !== pane.viewMode) {
                pane.queuedView = view
                if (compareDialog.visible)
                    compareDialog.close()
                else
                    surveyDialog.close()
            }
            return true
        }
        if (view === "grid") {
            const returningFromLoupe = pane.viewMode === "loupe"
            if (returningFromLoupe)
                editor.closeCompare()
            pane.pendingLoupeRow = -1
            pane.viewMode = "grid"
            if (returningFromLoupe && pane.visible && grid.currentIndex >= 0)
                grid.positionViewAtIndex(grid.currentIndex, GridView.Contain)
            return true
        }
        const selected = Array.from(library.reviewSelection(grid.currentIndex))
        if (view === "survey") {
            pane.viewMode = "survey"
            surveyDialog.showOrderedSelection(selected)
        } else if (view === "compare") {
            const primary = selected.shift()
            if (selected.length === 0) {
                const row = library.reviewPhotoRow(primary)
                const neighbor = row < 0 ? "" : library.reviewPhotoAt(row + 1) || library.reviewPhotoAt(row - 1)
                if (!neighbor)
                    return false
                selected.push(neighbor)
            }
            pane.viewMode = "compare"
            compareDialog.showCandidates(primary, selected)
        } else {
            const row = Math.max(0, grid.currentIndex)
            const id = selected.length ? selected[0] : library.reviewPhotoAt(row)
            if (!id)
                return false
            if (!selected.length)
                library.select(row)
            pane.showLoupe(id)
        }
        return true
    }
    function showLoupe(id) {
        pane.loupeID = id
        pane.loupeRow = library.reviewPhotoRow(id)
        if (pane.loupeRow >= 0)
            grid.currentIndex = pane.loupeRow
        pane.loupeCenterX = 0.5
        pane.loupeCenterY = 0.5
        loupeZoom.value = 1
        pane.viewMode = "loupe"
        editor.prepareLoupe(id)
    }
    function moveLoupe(direction) {
        const selected = Array.from(library.reviewSelection(-1))
        if (selected.length > 1) {
            const next = selected[selected.indexOf(pane.loupeID) + direction]
            if (next)
                pane.showLoupe(next)
        } else {
            const row = library.reviewPhotoRow(pane.loupeID)
            if (row >= 0 && row + direction >= 0 && row + direction < library.count) {
                pane.pendingLoupeRow = row + direction
                pane.finishLoupeNavigation()
            }
        }
    }
    function finishLoupeNavigation() {
        if (pane.pendingLoupeRow < 0 || pane.viewMode !== "loupe" || !pane.visible || library.working)
            return
        const id = library.reviewPhotoAt(pane.pendingLoupeRow)
        if (!id)
            return
        library.select(pane.pendingLoupeRow)
        pane.pendingLoupeRow = -1
        pane.showLoupe(id)
    }
    function openSelectedInDevelop() {
        if (!pane.canOpenInDevelop)
            return
        const id = pane.viewMode === "loupe" ? pane.loupeID : library.reviewSelection(grid.currentIndex)[0]
        if (id)
            library.openMapPhoto(id)
    }
    function requestImport() {
        if (!library.working)
            importDialog.open()
    }
    onVisibleChanged: if (!visible && viewMode === "loupe") requestView("grid")
    Shortcut {
        sequence: "Left"
        enabled: pane.visible && pane.viewMode === "loupe" && !pane.textInputFocused() && !library.working
        onActivated: pane.moveLoupe(-1)
    }
    Shortcut {
        sequence: "Right"
        enabled: pane.visible && pane.viewMode === "loupe" && !pane.textInputFocused() && !library.working
        onActivated: pane.moveLoupe(1)
    }
    Shortcut { sequence: "P"; enabled: pane.visible && pane.allowShortcut(); onActivated: library.flag(1) }
    Shortcut { sequence: "X"; enabled: pane.visible && pane.allowShortcut(); onActivated: library.flag(-1) }
    Shortcut { sequence: "U"; enabled: pane.visible && pane.allowShortcut(); onActivated: library.flag(0) }
    Repeater {
        model: 6
        Item { required property int index; Shortcut { sequence: index.toString(); enabled: pane.visible && pane.allowShortcut(); onActivated: library.rate(index) } }
    }
    Shortcut { sequences: [StandardKey.SelectAll]; enabled: pane.visible && !(search.activeFocus); onActivated: library.selectAll() }
    ImportDialog { id: importDialog }
    FolderDialog {
        id: captureFolderPicker
        title: qsTranslate("Apertone", "Folder used by the tethered capture app")
        onAccepted: library.startCaptureFolder(selectedFolder)
    }
    FolderDialog {
        id: cameraDestination
        title: qsTranslate("Apertone", "Choose a camera capture folder")
        onAccepted: cameraDialog.destination = selectedFolder
    }
    Dialog {
        id: cameraDialog
        objectName: "library.tetherDialog"
        anchors.centerIn: parent
        modal: true
        width: Math.min(580, parent.width - 32)
        title: qsTranslate("Apertone", "Tethered camera")
        property url destination
        closePolicy: library.cameraBusy ? Popup.NoAutoClose : Popup.CloseOnEscape
        onOpened: library.refreshCameras()
        contentItem: ColumnLayout {
            ApertoneComboBox {
                id: tetherCamera
                objectName: "library.tetherDevice"
                Layout.fillWidth: true
                model: library.tetherCameras; textRole: "name"; valueRole: "id"
                enabled: !library.working
                Accessible.name: qsTranslate("Apertone", "Tethered camera")
            }
            Label {
                text: qsTranslate("Apertone", "No compatible camera detected.")
                visible: !library.working && tetherCamera.count === 0 && !library.error
                Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            ApertoneButton {
                text: qsTranslate("Apertone", "Choose a camera capture folder")
                enabled: !library.working || library.cameraConnected
                onClicked: cameraDestination.open()
            }
            Label {
                text: cameraDialog.destination.toString()
                visible: !!text; elide: Text.ElideMiddle; Layout.fillWidth: true
            }
            RowLayout {
                visible: library.cameraConnected
                ApertoneButton {
                    visible: library.cameraLiveAvailable
                    text: library.cameraLive ? qsTranslate("Apertone", "Stop Live View")
                                             : qsTranslate("Apertone", "Start Live View")
                    onClicked: library.setCameraLive(!library.cameraLive)
                }
                ApertoneButton {
                    visible: library.cameraExposure.length > 0
                    text: qsTranslate("Apertone", "Refresh exposure")
                    onClicked: library.refreshCameraExposure()
                }
            }
            Image {
                objectName: "library.cameraLiveView"
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 225 : 0
                visible: library.cameraConnected && library.cameraLive &&
                         library.cameraFrame.toString().length > 0
                source: library.cameraFrame
                fillMode: Image.PreserveAspectFit
                cache: false
                smooth: true
            }
            Repeater {
                model: library.cameraConnected ? library.cameraExposure : []
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Label {
                        text: qsTranslate("Apertone", modelData.name)
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }
                    ApertoneComboBox {
                        Layout.preferredWidth: 150
                        model: modelData.choices
                        currentIndex: modelData.index
                        textRole: "text"
                        valueRole: "value"
                        visible: modelData.writable
                        Accessible.name: qsTranslate("Apertone", modelData.name)
                        onActivated: library.setCameraExposure(modelData.id, currentValue)
                    }
                    Label { visible: !modelData.writable; text: modelData.value }
                }
            }
            BusyIndicator { running: library.cameraBusy && !library.cameraConnected; visible: running }
            Label {
                text: qsTranslate("Apertone", library.error || library.status)
                visible: !!text; Layout.fillWidth: true; wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
        }
        footer: DialogButtonBox {
            ApertoneButton {
                text: library.cameraConnected ? qsTranslate("Apertone", "Disconnect camera")
                     : library.cameraBusy ? qsTranslate("Apertone", "Cancel") : qsTranslate("Apertone", "Close")
                onClicked: library.cameraBusy ? library.cancelCamera() : cameraDialog.close()
            }
            ApertoneButton {
                objectName: "library.tetherConnect"
                text: qsTranslate("Apertone", "Connect camera")
                visible: library.cameraControlsAvailable
                enabled: !library.working && tetherCamera.currentIndex >= 0
                onClicked: library.connectCameraControls(tetherCamera.currentValue)
            }
            ApertoneButton {
                text: qsTranslate("Apertone", "Refresh")
                enabled: !library.working
                onClicked: library.refreshCameras()
            }
            ApertoneButton {
                objectName: "library.tetherShutter"
                text: qsTranslate("Apertone", "Take photo")
                enabled: (!library.working || library.cameraConnected) && tetherCamera.currentIndex >= 0 && !!cameraDialog.destination.toString()
                onClicked: library.captureWithCamera(tetherCamera.currentValue, cameraDialog.destination)
            }
        }
    }
    Dialog {
        id: virtualCopyName
        modal: true
        anchors.centerIn: parent
        width: 390
        title: qsTranslate("Apertone", renaming ? "Rename virtual copy" : "Create virtual copy")
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string assetID: ""
        property bool renaming: false
        function showName(rename) {
            assetID = library.selectedAssetIDs()[0]
            renaming = rename
            copyNameInput.text = rename ? library.selectedCopyName : qsTranslate("Apertone", "Virtual copy")
            open()
        }
        onOpened: { copyNameInput.forceActiveFocus(); copyNameInput.selectAll() }
        onAccepted: {
            if (copyNameInput.text.trim().length === 0)
                return
            if (renaming)
                library.renameVirtualCopy(assetID, copyNameInput.text)
            else
                library.createVirtualCopy(assetID, copyNameInput.text)
        }
        ApertoneTextField {
            id: copyNameInput
            width: parent.width
            maximumLength: 120
            selectByMouse: true
            Accessible.name: qsTranslate("Apertone", "Virtual copy name")
            onAccepted: if (text.trim().length > 0) virtualCopyName.accept()
        }
    }
    Dialog {
        id: removeVirtualCopy
        modal: true
        anchors.centerIn: parent
        width: 390
        property string assetID: ""
        title: qsTranslate("Apertone", "Delete virtual copy?")
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTranslate("Apertone", "This removes the copy and its edits from the catalog.")
        }
        onAccepted: {
            if (editor.photoID !== assetID || editor.closePhoto())
                library.deleteVirtualCopy(assetID)
        }
    }
    Dialog {
        id: collectionDialog; objectName: "library.collectionDialog"; width: 360; implicitWidth: 360; title: pane.editCollectionID ? qsTranslate("Apertone", "Rename collection") : qsTranslate("Apertone", "New collection"); modal: true; anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            standardButton(Dialog.Ok).objectName = "library.collectionConfirm"
            standardButton(Dialog.Cancel).objectName = "library.collectionCancel"
            collectionParent.currentIndex = Math.max(0, collectionParent.indexOfValue(pane.currentSet))
            collectionName.forceActiveFocus()
        }
        ColumnLayout {
            width: parent.width
            ApertoneTextField { id: collectionName; objectName: "library.collectionName"; Layout.fillWidth: true; placeholderText: qsTranslate("Apertone", "Collection name"); maximumLength: 128; selectByMouse: true }
            ApertoneComboBox {
                id: collectionParent
                objectName: "library.collectionParent"
                visible: pane.setChoices.length > 1
                Layout.fillWidth: true; Layout.minimumWidth: 0
                model: pane.setChoices; textRole: "path"; valueRole: "id"
                Accessible.name: qsTranslate("Apertone", "Collection set")
            }
        }
        onAccepted: {
            const destination = collectionParent.currentValue || ""
            if (pane.editCollectionID)
                library.updateCollection(pane.editCollectionID, collectionName.text, destination)
            else
                library.createCollection(collectionName.text, destination)
        }
    }
    Dialog {
        id: deleteDialog; objectName: "library.deleteCollectionDialog"; width: 420; implicitWidth: 420; title: qsTranslate("Apertone", "Delete collection?"); modal: true; anchors.centerIn: parent; standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: { standardButton(Dialog.Ok).objectName = "library.deleteConfirm"; standardButton(Dialog.Cancel).objectName = "library.deleteCancel" }
        Label { text: qsTranslate("Apertone", "Photos and edits will remain in the catalog.") }
        onAccepted: { library.deleteCollection(pane.currentCollection); pane.currentCollection = ""; pane.applyFilter() }
    }
    Dialog {
        id: reportDialog; objectName: "library.importReportDialog"; title: qsTranslate("Apertone", "Last import errors"); modal: true; anchors.centerIn: parent
        width: Math.min(pane.width - 40, 700); height: Math.min(pane.height - 40, 450)
        standardButtons: Dialog.Close
        ScrollView { objectName: "library.importReportScroll"; anchors.fill: parent; clip: true; TextArea { objectName: "library.importReportText"; text: library.importReport; readOnly: true; wrapMode: Text.WrapAnywhere; textFormat: TextEdit.PlainText; selectByMouse: true } }
    }
    Dialog {
        id: duplicateReviewDialog; objectName: "library.duplicateReviewDialog"
        title: qsTranslate("Apertone", "Skipped exact duplicates")
        modal: true; anchors.centerIn: parent
        width: Math.min(pane.width - 40, 720); height: Math.min(pane.height - 40, 500)
        standardButtons: Dialog.Close
        ColumnLayout {
            anchors.fill: parent; spacing: Theme.panelSpacing
            Label {
                text: qsTranslate("Apertone", "These files matched existing originals byte for byte. Apertone kept the existing catalog item and did not delete either file.")
                wrapMode: Text.Wrap; Layout.fillWidth: true
            }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                Column {
                    width: parent.width; spacing: Theme.panelSpacing
                    Repeater {
                        model: library.duplicateReview
                        Rectangle {
                            required property var modelData
                            width: parent.width; implicitHeight: duplicateText.implicitHeight + 16
                            color: Theme.control; radius: 6
                            Label {
                                id: duplicateText; anchors.fill: parent; anchors.margins: 8
                                text: modelData.sourceName + "\n" + modelData.sourcePath + "\n→ " +
                                      modelData.existingName + "\n" + modelData.existingPath
                                color: Theme.text; font.pixelSize: 11; wrapMode: Text.WrapAnywhere
                                textFormat: Text.PlainText
                            }
                        }
                    }
                }
            }
        }
    }
    FingerprintReviewDialog { id: fingerprintDialog; libraryModel: library }
    FileDialog {
        id: lightroomFile
        title: qsTranslate("Apertone", "Lightroom catalog")
        nameFilters: ["Lightroom Classic (*.lrcat *.LRCAT)"]
        onAccepted: {
            lightroomDialog.source = selectedFile
            lightroomDialog.profiles = ""
            rebuildLightroomMasks.checked = false
            library.inspectLightroom(selectedFile)
            lightroomDialog.open()
        }
    }
    FolderDialog {
        id: lightroomProfiles
        title: qsTranslate("Apertone", "Choose a DNG camera profiles folder")
        onAccepted: lightroomDialog.profiles = selectedFolder
    }
    Dialog {
        id: lightroomDialog
        objectName: "library.lightroomImport"
        title: qsTranslate("Apertone", "Lightroom catalog")
        property url source
        property url profiles
        readonly property var report: library.lightroomReport
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: parent
        width: Math.min(590, parent.width - 60)
        contentItem: ColumnLayout {
            Label {
                visible: library.importing
                text: qsTranslate("Apertone", library.status)
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            ProgressBar {
                visible: library.importing
                value: library.progress
                indeterminate: value === 0
                Layout.fillWidth: true
            }
            Label {
                visible: Boolean(lightroomDialog.report.fingerprint)
                text: qsTranslate("Apertone", "%1 photos · %2 available · %3 missing · %4 unsupported")
                      .arg(lightroomDialog.report.photos || 0).arg(lightroomDialog.report.available || 0)
                      .arg(lightroomDialog.report.missing || 0).arg(lightroomDialog.report.unsupported || 0)
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            Label {
                visible: Boolean(lightroomDialog.report.fingerprint)
                text: qsTranslate("Apertone", "%1 edits mapped · %2 partial · %3 unmapped · %4 virtual copies")
                      .arg(lightroomDialog.report.mappedEdits || 0).arg(lightroomDialog.report.partialEdits || 0)
                      .arg(lightroomDialog.report.unmappedEdits || 0).arg(lightroomDialog.report.virtualCopies || 0)
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            Label {
                visible: Boolean(lightroomDialog.report.fingerprint) && lightroomDialog.report.smartCollections > 0
                text: qsTranslate("Apertone", "Smart Collection rules mapped: %1 of %2")
                      .arg(lightroomDialog.report.smartRulesMapped || 0).arg(lightroomDialog.report.smartCollections || 0)
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            RowLayout {
                visible: Boolean(lightroomDialog.report.fingerprint) && !lightroomDialog.report.applied
                Layout.fillWidth: true
                ApertoneButton {
                    objectName: "library.lightroomProfiles"
                    text: qsTranslate("Apertone", "Camera profiles folder")
                    onClicked: lightroomProfiles.open()
                }
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideLeft
                    textFormat: Text.PlainText
                    text: lightroomDialog.profiles.toString()
                }
            }
            ApertoneCheckBox {
                id: rebuildLightroomMasks
                objectName: "library.rebuildLightroomMasks"
                visible: Boolean(lightroomDialog.report.fingerprint) && !lightroomDialog.report.applied
                text: qsTranslate("Apertone", "Rebuild missing AI masks locally")
            }
            Label {
                visible: Boolean(lightroomDialog.report.applied)
                text: qsTranslate("Apertone", "%1 added · %2 processing versions · %3 already imported · %4 unreadable")
                      .arg(lightroomDialog.report.added || 0).arg(lightroomDialog.report.versions || 0)
                      .arg(lightroomDialog.report.unchanged || 0).arg(lightroomDialog.report.unreadable || 0)
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            Label {
                visible: Boolean(lightroomDialog.report.applied) && lightroomDialog.report.copies > 0
                text: qsTranslate("Apertone", "%1 virtual copies created").arg(lightroomDialog.report.copies || 0)
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            Label {
                visible: Boolean(lightroomDialog.report.applied) && (lightroomDialog.report.historyMapped > 0 || lightroomDialog.report.historySkipped > 0)
                text: qsTranslate("Apertone", "%1 Develop states imported · %2 partially mapped · %3 skipped")
                      .arg(lightroomDialog.report.historySteps || 0).arg(lightroomDialog.report.historyPartial || 0)
                      .arg(lightroomDialog.report.historySkipped || 0)
                Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            Label {
                visible: Boolean(lightroomDialog.report.applied) && (lightroomDialog.report.snapshotsMapped > 0 || lightroomDialog.report.snapshotsSkipped > 0)
                text: qsTranslate("Apertone", "%1 Develop snapshots imported · %2 partially mapped · %3 skipped")
                      .arg(lightroomDialog.report.snapshots || 0).arg(lightroomDialog.report.snapshotsPartial || 0)
                      .arg(lightroomDialog.report.snapshotsSkipped || 0)
                Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            Label {
                visible: Boolean(lightroomDialog.report.applied) && (lightroomDialog.report.masksMapped > 0 || lightroomDialog.report.masksSkipped > 0)
                text: qsTranslate("Apertone", "%1 Lightroom masks mapped · %2 skipped")
                      .arg(lightroomDialog.report.masksMapped || 0).arg(lightroomDialog.report.masksSkipped || 0)
                Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            Label {
                visible: Boolean(lightroomDialog.report.applied) && (lightroomDialog.report.masksRecovered > 0 || lightroomDialog.report.masksRebuilt > 0)
                text: qsTranslate("Apertone", "%1 masks recovered from history · %2 rebuilt")
                      .arg(lightroomDialog.report.masksRecovered || 0).arg(lightroomDialog.report.masksRebuilt || 0)
                Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            Label {
                visible: Boolean(lightroomDialog.report.applied) && (lightroomDialog.report.profilesMapped > 0 || lightroomDialog.report.profilesSkipped > 0)
                text: qsTranslate("Apertone", "%1 DNG camera profiles mapped · %2 skipped")
                      .arg(lightroomDialog.report.profilesMapped || 0).arg(lightroomDialog.report.profilesSkipped || 0)
                Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            Label {
                visible: library.error.length > 0
                text: qsTranslate("Apertone", library.error)
                textFormat: Text.PlainText
                color: Theme.negative
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            ScrollView {
                visible: Boolean(lightroomDialog.report.warnings && lightroomDialog.report.warnings.length)
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 170 : 0
                contentWidth: availableWidth
                Label {
                    text: lightroomDialog.report.warnings ? lightroomDialog.report.warnings.join("\n") : ""
                    width: parent.width
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                }
            }
        }
        footer: DialogButtonBox {
            ApertoneButton {
                text: qsTranslate("Apertone", library.importing ? "Cancel" : "Close")
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: library.importing ? library.cancelImport() : lightroomDialog.close()
            }
            ApertoneButton {
                objectName: "library.applyLightroom"
                text: qsTranslate("Apertone", "Import")
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                enabled: !library.working && !lightroomDialog.report.applied &&
                         (lightroomDialog.report.available > 0 || lightroomDialog.report.manualCollections > 0 ||
                          lightroomDialog.report.smartRulesMapped > 0)
                onClicked: library.applyLightroom(lightroomDialog.source, lightroomDialog.report.fingerprint,
                                                   lightroomDialog.profiles, rebuildLightroomMasks.checked)
            }
        }
    }
    RowLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            objectName: "library.leftPanel"
            visible: pane.leftPanelVisible
            color: Theme.panel; Layout.preferredWidth: Theme.sidebarWidth; Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent; anchors.margins: Theme.panelPadding; spacing: Theme.panelSpacing
                Label { text: qsTranslate("Apertone", "LIBRARY"); font.pixelSize: 10; color: Theme.faint; font.letterSpacing: 1.5 }
                ApertoneButton { objectName: "library.importButton"; text: qsTranslate("Apertone", "+  Import"); Layout.fillWidth: true; enabled: !library.working; onClicked: pane.requestImport() }
                ApertoneButton {
                    objectName: "library.cardImport"
                    text: qsTranslate("Apertone", "Import from card")
                    Layout.fillWidth: true; enabled: !library.working
                    onClicked: { cards.devices = library.cardSources(); cards.open() }
                    Menu {
                        id: cards
                        property var devices: []
                        MenuItem { visible: !cards.devices.length; enabled: false; text: qsTranslate("Apertone", "No mounted card with a DCIM folder") }
                        Repeater {
                            model: cards.devices
                            MenuItem { required property var modelData; text: modelData.name; onTriggered: importDialog.openCard(modelData.url) }
                        }
                    }
                }
                ApertoneButton {
                    objectName: "library.captureFolder"
                    Layout.fillWidth: true
                    text: library.captureFolder ? qsTranslate("Apertone", "Stop watching capture folder") : qsTranslate("Apertone", "Watch capture folder…")
                    enabled: !!library.captureFolder || !library.working
                    onClicked: library.captureFolder ? library.stopCaptureFolder() : captureFolderPicker.open()
                }
                ApertoneButton {
                    objectName: "library.tetherOpen"
                    text: qsTranslate("Apertone", "Tethered camera…")
                    visible: library.tetherAvailable
                    Layout.fillWidth: true; enabled: !library.working
                    onClicked: cameraDialog.open()
                }
                ApertoneButton { objectName: "library.lightroomButton"; text: qsTranslate("Apertone", "Import Lightroom catalog…"); Layout.fillWidth: true; enabled: !library.working; onClicked: lightroomFile.open() }
                ApertoneButton { text: qsTranslate("Apertone", "All photos"); Layout.fillWidth: true; highlighted: !pane.currentCollection; onClicked: { pane.currentCollection = ""; pane.applyFilter() } }
                ApertoneButton { objectName: "library.fingerprintOpen"; text: qsTranslate("Apertone", "Catalog matches"); Layout.fillWidth: true; enabled: !library.working && !library.loading; onClicked: fingerprintDialog.open() }
                ApertoneButton {
                    objectName: "semantic.open"
                    text: semanticSearch.categorySearch ? qsTranslate("Apertone", "Search by category") : qsTranslate("Apertone", "Search by description")
                    Layout.fillWidth: true
                    onClicked: semanticSearch.open()
                }
                ApertoneButton {
                    objectName: "library.photoActions"
                    text: qsTranslate("Apertone", "Photo actions")
                    Layout.fillWidth: true
                    onClicked: photoActions.open()
                    Menu {
                        id: photoActions
                        MenuItem { objectName: "library.photoMetadata"; text: qsTranslate("Apertone", "Photo metadata"); enabled: library.selectedCount === 1 && !library.working; onTriggered: photoMetadata.showPhoto(library.selectedAssetIDs()[0]) }
                        MenuItem { objectName: "library.preparePreviews"; text: qsTranslate("Apertone", "Prepare offline previews"); enabled: library.selectedCount > 0 && !library.working && !library.selecting; onTriggered: library.prepareOfflinePreviews() }
                        MenuItem { objectName: "library.delivery"; text: qsTranslate("Apertone", "Deliver selected photos…"); enabled: library.selectedCount > 0 && !library.working && !library.selecting && !editor.saving; onTriggered: deliveryDialog.open() }
                        MenuItem { objectName: "library.map"; text: qsTranslate("Apertone", "Show photo locations…"); enabled: library.selectedCount > 0 && !library.working && !library.selecting; onTriggered: photoMapDialog.open() }
                        MenuItem { objectName: "library.pinReference"; text: qsTranslate("Apertone", "Pin Develop reference"); enabled: library.selectedCount === 1 && !library.working && !editor.referencePhoto.busy; onTriggered: editor.pinReference(library.selectedAssetIDs()[0]) }
                        MenuItem { objectName: "library.createStack"; text: qsTranslate("Apertone", "Stack selected photos"); enabled: library.selectedCount >= 2 && !library.working && !library.selecting; onTriggered: library.stackSelection() }
                        MenuItem { text: qsTranslate("Apertone", "Show photos in stack"); enabled: !!library.selectedStackID && !library.viewingStack && !library.working; onTriggered: library.viewStack(library.selectedStackID) }
                        MenuItem { text: qsTranslate("Apertone", "Set stack cover"); enabled: !!library.selectedStackID && !library.working; onTriggered: library.setSelectedStackCover() }
                        MenuItem { text: qsTranslate("Apertone", "Ungroup stack"); enabled: !!library.selectedStackID && !library.working; onTriggered: library.unstackSelection() }
                        MenuItem {
                            objectName: "library.createVirtualCopy"
                            text: qsTranslate("Apertone", "Create virtual copy…")
                            enabled: library.selectedCount === 1 && !library.working && !editor.busy && !editor.saving
                            onTriggered: virtualCopyName.showName(false)
                        }
                        MenuItem {
                            text: qsTranslate("Apertone", "Rename virtual copy…")
                            visible: library.selectedCopyName.length > 0
                            enabled: !library.working && !editor.busy
                            onTriggered: virtualCopyName.showName(true)
                        }
                        MenuItem {
                            text: qsTranslate("Apertone", "Delete virtual copy…")
                            visible: library.selectedCopyName.length > 0
                            enabled: !library.working && !editor.busy && !editor.saving
                            onTriggered: {
                                removeVirtualCopy.assetID = library.selectedAssetIDs()[0]
                                removeVirtualCopy.open()
                            }
                        }
                        MenuItem { text: qsTranslate("Apertone", "Compare Select / Candidate"); enabled: pane.canRequestView("compare"); onTriggered: pane.requestView("compare") }
                        MenuItem {
                            objectName: "library.openSurvey"
                            text: qsTranslate("Apertone", "Survey")
                            enabled: pane.canSurvey
                            onTriggered: pane.requestView("survey")
                        }
                        MenuItem { objectName: "library.hdr"; text: qsTranslate("Apertone", "HDR merge"); enabled: library.selectedCount >= 2 && library.selectedCount <= 9 && !library.working && !editor.aiBusy && !editor.busy; onTriggered: hdrDialog.open() }
                        MenuItem { objectName: "library.panorama"; text: qsTranslate("Apertone", "Panorama merge"); enabled: library.selectedCount >= 2 && library.selectedCount <= 24 && !library.working && !editor.aiBusy && !editor.busy; onTriggered: panoramaDialog.open() }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; Layout.topMargin: 8
                    Label { text: qsTranslate("Apertone", "COLLECTIONS"); font.pixelSize: 10; color: Theme.faint; Layout.fillWidth: true }
                    ApertoneButton {
                        objectName: "library.collectionSets"
                        text: qsTranslate("Apertone", "Sets…")
                        enabled: !library.working
                        onClicked: collectionSets.edit(pane.currentSet)
                    }
                }
                ApertoneButton {
                    objectName: "library.keywordTree"
                    text: qsTranslate("Apertone", "Keywords…")
                    Layout.fillWidth: true
                    enabled: !library.working
                    onClicked: keywordEditor.open()
                }
                ApertoneComboBox {
                    id: setFilter
                    objectName: "library.collectionSetFilter"
                    visible: pane.setChoices.length > 1
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    model: pane.setChoices; textRole: "path"; valueRole: "id"
                    currentIndex: Math.max(0, indexOfValue(pane.currentSet))
                    Accessible.name: qsTranslate("Apertone", "Collection set")
                    onActivated: {
                        pane.currentCollection = ""
                        pane.currentSet = currentValue || ""
                        pane.applyFilter()
                    }
                }
                ScrollView {
                    objectName: "library.collectionsScroll"
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
                    Column {
                        width: parent.width; spacing: 4
                        Repeater {
                            model: library.collections.filter(row => row.set === pane.currentSet)
                            ApertoneButton { required property var modelData; objectName: "library.collection." + modelData.id; width: parent.width; text: modelData.name + " · " + modelData.count; highlighted: pane.currentCollection === modelData.id; onClicked: { pane.currentCollection = modelData.id; pane.applyFilter() } }
                        }
                    }
                }
                ApertoneButton {text:qsTranslate("Apertone","+ Smart Collection");Layout.fillWidth:true;enabled:!library.working;onClicked:smartCollection.create(pane.currentSet)}
                ApertoneButton {
                    text:qsTranslate("Apertone","Edit rules");Layout.fillWidth:true;enabled:!library.working
                    visible:!!pane.currentCollection && library.collections.some(c=>c.id===pane.currentCollection&&c.smart)
                    onClicked:{const c=library.collections.find(c=>c.id===pane.currentCollection);if(c)library.editSmartCollection(c.id,c.name)}
                }
                ApertoneButton { objectName: "library.newCollection"; text: qsTranslate("Apertone", "+ Collection"); Layout.fillWidth: true; enabled: !library.working; onClicked: { pane.editCollectionID = ""; collectionName.text = ""; collectionDialog.open() } }
                ApertoneButton { objectName: "library.renameCollection"; text: qsTranslate("Apertone", "Rename"); visible: !!pane.currentCollection; enabled: !library.working; Layout.fillWidth: true; onClicked: { pane.editCollectionID = pane.currentCollection; const item = library.collections.find(c => c.id === pane.currentCollection); collectionName.text = item ? item.name : ""; collectionDialog.open() } }
                ApertoneButton { objectName: "library.deleteCollection"; text: qsTranslate("Apertone", "Delete collection"); visible: !!pane.currentCollection; enabled: !library.working; Layout.fillWidth: true; onClicked: deleteDialog.open() }
                Label {
                    text: {
                        const keys = qsTranslate("Apertone", "Double-click → Develop\n⌘ / Shift → select\n0–5 · P / X / U")
                        return Qt.platform.os === "windows" ? keys.replace("⌘", "Ctrl") : keys
                    }
                    color: Theme.faint; font.pixelSize: 10; Layout.topMargin: 8
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.fillHeight: true; Layout.margins: Theme.panelPadding; spacing: Theme.panelSpacing
            RowLayout {
                Layout.minimumWidth: 0; Layout.fillWidth: true
                ApertoneTextField { id: search; objectName: "library.search"; Layout.minimumWidth: 0; placeholderTextColor: Theme.muted; placeholderText: qsTranslate("Apertone", "Search: name, path, camera, lens"); selectByMouse: true; maximumLength: 1024; Layout.fillWidth: true; onTextChanged: pane.applyFilter() }
                ApertoneComboBox { id: ratingFilter; objectName: "library.ratingFilter"; Layout.preferredWidth: 82; model: [qsTranslate("Apertone", "All ★"), "≥ 1 ★", "≥ 2 ★", "≥ 3 ★", "≥ 4 ★", "5 ★"]; onActivated: pane.applyFilter() }
                ApertoneComboBox { id: flagFilter; objectName: "libraryFlagFilter"; Layout.preferredWidth: 114; ToolTip.visible: hovered; ToolTip.text: currentText; model: [qsTranslate("Apertone", "All flags"), qsTranslate("Apertone", "Reject"), qsTranslate("Apertone", "Unflagged"), qsTranslate("Apertone", "Pick")]; onActivated: pane.applyFilter() }
                ApertoneComboBox { id: labelFilter; objectName: "libraryLabelFilter"; Layout.preferredWidth: 114; ToolTip.visible: hovered; ToolTip.text: currentText; model: [qsTranslate("Apertone", "All labels"), qsTranslate("Apertone", "No label"), qsTranslate("Apertone", "Red"), qsTranslate("Apertone", "Yellow"), qsTranslate("Apertone", "Green"), qsTranslate("Apertone", "Blue"), qsTranslate("Apertone", "Purple")]; onActivated: pane.applyFilter() }
                ApertoneComboBox {
                    objectName: "library.sortOrder"
                    Layout.preferredWidth: 162
                    enabled: !library.working
                    currentIndex: library.sortOrder
                    Accessible.name: qsTranslate("Apertone", "Sort photos")
                    ToolTip.visible: hovered
                    ToolTip.text: currentText
                    model: [
                        qsTranslate("Apertone", "Filename"),
                        qsTranslate("Apertone", "Capture time ↓"),
                        qsTranslate("Apertone", "Capture time ↑"),
                        qsTranslate("Apertone", "Highest rating")
                    ]
                    onActivated: function(index) {
                        pane.requestView("grid")
                        library.sortOrder = index
                    }
                }
            }
            GridLayout {
                objectName: "intelligence.controls"
                Layout.minimumWidth: 0; Layout.fillWidth: true; columnSpacing: 6; rowSpacing: 5
                columns: width < 880 ? 4 : 9
                Label { text: qsTranslate("Apertone", "INTELLIGENT"); font.pixelSize: 9; color: Theme.faint; font.letterSpacing: 1.2 }
                ApertoneComboBox {
                    id: suggestionFilter; objectName: "intelligence.suggestionFilter"
                    Layout.preferredWidth: 116
                    model: [qsTranslate("Apertone", "All photos"), qsTranslate("Apertone", "Pick"), qsTranslate("Apertone", "Review"), qsTranslate("Apertone", "Reject")]
                    onActivated: pane.applyIntelligentFilter()
                }
                ApertoneComboBox {
                    id: faceGroupFilter; objectName: "intelligence.faceGroupFilter"
                    Layout.preferredWidth: 156; model: pane.intelligenceGroups; textRole: "displayName"
                    onActivated: { pane.intelligentFaceGroupID = currentIndex > 0 ? model[currentIndex].id : ""; pane.applyIntelligentFilter() }
                }
                ApertoneButton {
                    objectName: "intelligence.renameFaceGroup"; text: qsTranslate("Apertone", "Name")
                    visible: faceGroupFilter.currentIndex > 0; enabled: !library.working
                    onClicked: faceGroupDialog.open()
                }
                ApertoneCheckBox {
                    id: bestOfBurst; objectName: "intelligence.bestOfBurst"
                    text: qsTranslate("Apertone", "Best of burst"); font.pixelSize: 11
                    onToggled: pane.applyIntelligentFilter()
                }
                ApertoneCheckBox {
                    objectName: "intelligence.autoAnalyze"
                    text: qsTranslate("Apertone", "Analyze new imports locally"); font.pixelSize: 11
                    checked: library.cullingAutoEnabled
                    onClicked: library.cullingAutoEnabled = checked
                }
                Label {
                    visible: library.cullingSummary.analyzed > 0
                    text: library.cullingSummary.picks + " " + qsTranslate("Apertone", "picks") + " · " +
                          library.cullingSummary.faceGroups + " " + qsTranslate("Apertone", "face groups")
                    color: Theme.faint; font.pixelSize: 10
                }
                ApertoneButton {
                    objectName: "intelligence.apply"; text: library.selectedCount > 0 ? qsTranslate("Apertone", "Apply to selection") : qsTranslate("Apertone", "Apply suggestions")
                    enabled: !library.working && library.cullingSummary.analyzed > 0 && (library.selectedCount > 0 || library.count > 0)
                    onClicked: applySuggestionsDialog.open()
                }
                ApertoneButton {
                    objectName: "intelligence.analyze"; text: library.selectedCount > 0 ? qsTranslate("Apertone", "Analyze selection") : qsTranslate("Apertone", "Analyze library")
                    enabled: !library.working && library.count > 0
                    onClicked: library.analyzeLibrary()
                }
                ApertoneButton {
                    objectName: "intelligence.clear"; text: qsTranslate("Apertone", "Clear analysis")
                    enabled: !library.working && library.cullingSummary.analyzed > 0
                    onClicked: clearAnalysisDialog.open()
                }
            }
            RowLayout {
                visible: library.cullingBusy || library.cullingStatus.length > 0 || library.cullingError.length > 0
                Layout.fillWidth: true; spacing: 6
                ProgressBar { visible: library.cullingBusy; value: library.cullingProgress; Layout.fillWidth: true }
                Label { visible: !library.cullingBusy && library.cullingError.length === 0; text: qsTranslate("Apertone", library.cullingStatus); color: Theme.faint; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
                Label { visible: library.cullingError.length > 0; text: qsTranslate("Apertone", library.cullingError); color: Theme.negative; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
                ApertoneButton { visible: library.cullingBusy; text: qsTranslate("Apertone", "Cancel"); onClicked: library.cancelCulling() }
            }
            RowLayout {
                visible: library.importing || library.preparingPreviews; Layout.fillWidth: true
                ProgressBar { value: library.progress; indeterminate: library.progress === 0; Layout.fillWidth: true }
                ApertoneButton { objectName: "library.cancelImport"; visible: library.importing; text: qsTranslate("Apertone", "Cancel import"); onClicked: library.cancelImport() }
                ApertoneButton { visible: library.preparingPreviews; text: qsTranslate("Apertone", "Cancel"); onClicked: library.cancelOfflinePreviews() }
            }
            Label { objectName: "library.error"; visible: library.error.length > 0; text: qsTranslate("Apertone", library.error); color: Theme.negative; wrapMode: Text.Wrap; Layout.fillWidth: true }
            RowLayout {
                Layout.minimumWidth: 0; Layout.fillWidth: true
                ApertoneButton { objectName: "library.importReportButton"; visible: library.importReport.length > 0; text: qsTranslate("Apertone", "Import errors"); onClicked: reportDialog.open() }
                ApertoneButton { objectName: "library.duplicateReviewButton"; visible: library.duplicateReview.length > 0; text: qsTranslate("Apertone", "Duplicates") + " · " + library.duplicateReview.length; onClicked: duplicateReviewDialog.open() }
                Label { text: library.loading ? qsTranslate("Apertone", "Loading…") : library.count + qsTranslate("Apertone", " photos · selected ") + library.selectedCount; color: Theme.muted; Layout.fillWidth: true }
                ApertoneButton { visible: library.viewingStack; text: qsTranslate("Apertone", "Exit stack"); enabled: !library.working; onClicked: library.viewStack("") }
                ApertoneCheckBox { visible: !library.viewingStack; checked: library.collapseStacks; enabled: !library.working; text: qsTranslate("Apertone", "Collapse stacks"); onToggled: library.collapseStacks = checked }
                ApertoneButton { objectName: "library.selectAll"; text: qsTranslate("Apertone", "Select all"); enabled: library.count > 0; onClicked: library.selectAll() }
                ApertoneButton { objectName: "library.clearSelection"; text: qsTranslate("Apertone", "Clear selection"); enabled: library.selectedCount > 0; onClicked: library.clearSelection() }
            }
            BatchSettings {}
            RowLayout {
                visible: editor.aiBusy && (editor.aiOperation === "HDR merge" || editor.aiOperation === "Panorama merge")
                Layout.fillWidth: true
                Label { objectName: "merge.operation"; text: qsTranslate("Apertone", editor.aiOperation) }
                Label {
                    objectName: "merge.phase"
                    text: editor.aiProgress < 0.25 ? qsTranslate("Apertone", "Aligning originals…")
                        : editor.aiProgress < 0.85 ? qsTranslate("Apertone", "Merging frames…")
                        : qsTranslate("Apertone", "Writing Float DNG…")
                }
                ProgressBar { objectName: "merge.progress"; value: editor.aiProgress; Layout.fillWidth: true }
                ApertoneButton { objectName: "merge.cancel"; text: qsTranslate("Apertone", "Cancel"); onClicked: editor.cancelAI() }
            }
            Label { text: qsTranslate("Apertone", batch.status); textFormat: Text.PlainText; visible: text.length > 0; color: Theme.muted; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
            Label { text: qsTranslate("Apertone", batch.error); textFormat: Text.PlainText; visible: text.length > 0; color: Theme.negative; wrapMode: Text.Wrap; Layout.fillWidth: true }
            RowLayout {
                objectName: "library.viewToolbar"
                visible: pane.viewToolbarVisible
                Layout.fillWidth: true
                Repeater {
                    model: [
                        {view: "grid", title: qsTranslate("Apertone", "Grid"), key: "G"},
                        {view: "loupe", title: qsTranslate("Apertone", "Loupe"), key: "E"},
                        {view: "compare", title: qsTranslate("Apertone", "Compare"), key: "C"},
                        {view: "survey", title: qsTranslate("Apertone", "Survey"), key: "N"}
                    ]
                    ApertoneButton {
                        required property var modelData
                        objectName: "library.view." + modelData.view
                        text: modelData.title
                        highlighted: pane.viewMode === modelData.view
                        enabled: pane.canRequestView(modelData.view)
                        ToolTip.visible: hovered
                        ToolTip.text: modelData.title + " (" + modelData.key + ")"
                        onClicked: pane.requestView(modelData.view)
                    }
                }
                Item { Layout.fillWidth: true }
                ApertoneButton {
                    objectName: "library.loupe.previous"
                    visible: pane.viewMode === "loupe"
                    enabled: !library.working
                    text: "‹"
                    Accessible.name: qsTranslate("Apertone", "Previous")
                    onClicked: pane.moveLoupe(-1)
                }
                ApertoneButton {
                    objectName: "library.loupe.next"
                    visible: pane.viewMode === "loupe"
                    enabled: !library.working
                    text: "›"
                    Accessible.name: qsTranslate("Apertone", "Next")
                    onClicked: pane.moveLoupe(1)
                }
                ApertoneButton {
                    visible: pane.viewMode === "loupe"
                    text: qsTranslate("Apertone", "Develop")
                    enabled: pane.canOpenInDevelop
                    onClicked: pane.openSelectedInDevelop()
                }
            }
            Item {
                Layout.fillWidth: true; Layout.fillHeight: true
                GridView {
                    id: grid; objectName: "library.grid"; anchors.fill: parent; clip: true; model: library
                    visible: pane.viewMode !== "loupe"
                    currentIndex: -1
                    property string viewportAsset: ""
                    property real viewportOffset: 0
                    property bool restorePending: false
                    property int restoreRevision: 0
                    property int geometryAnchorRow: -1
                    property real geometryAnchorOffset: 0
                    property bool geometryRestorePending: false
                    property int geometryRevision: 0
                    function firstViewportCard() {
                        let card = null
                        for (const candidate of contentItem.children) {
                            if (candidate instanceof PhotoCard && candidate.assetID.length &&
                                candidate.y + candidate.height > contentY && candidate.y < contentY + height &&
                                (!card || candidate.index < card.index))
                                card = candidate
                        }
                        return card
                    }
                    function captureGeometryAnchor() {
                        if (restorePending || geometryRestorePending || library.loading || moving || libraryBar.pressed)
                            return
                        const card = firstViewportCard()
                        geometryAnchorRow = card ? card.index : -1
                        geometryAnchorOffset = card ? card.y - contentY : 0
                    }
                    function cancelGeometryRestore() {
                        ++geometryRevision
                        geometryRestorePending = false
                    }
                    function restoreGeometryAnchor() {
                        if (geometryAnchorRow < 0 || restorePending || library.loading || moving || libraryBar.pressed)
                            return
                        const revision = ++geometryRevision
                        geometryRestorePending = true
                        Qt.callLater(function() {
                            if (revision !== geometryRevision)
                                return
                            geometryRestorePending = false
                            if (restorePending || library.loading || geometryAnchorRow >= count)
                                return
                            forceLayout()
                            positionViewAtIndex(geometryAnchorRow, GridView.Beginning)
                            contentY = Math.max(originY, Math.min(originY + Math.max(0, contentHeight - height), contentY - geometryAnchorOffset))
                        })
                    }
                    function cancelViewportRestore() {
                        ++restoreRevision
                        restorePending = false
                    }
                    function captureViewport(preserve) {
                        cancelGeometryRestore()
                        geometryAnchorRow = -1
                        ++restoreRevision
                        if (!preserve || dragging || libraryBar.pressed) {
                            restorePending = false
                            viewportAsset = ""
                        } else {
                            const card = firstViewportCard()
                            if (card && card.assetID.length) {
                                viewportAsset = card.assetID
                                viewportOffset = card.y - contentY
                                restorePending = true
                            } else if (!restorePending) {
                                // Keep a pending identity across another reload before
                                // delegates for the preceding result have been laid out.
                                viewportAsset = ""
                            }
                        }
                        library.setViewportAnchor(restorePending ? viewportAsset : "")
                    }
                    function restoreViewport(row) {
                        const revision = restoreRevision
                        Qt.callLater(function() {
                            if (revision !== restoreRevision || !restorePending)
                                return
                            restorePending = false
                            if (row < 0 || row >= count) {
                                captureGeometryAnchor()
                                return
                            }
                            cancelFlick()
                            forceLayout()
                            positionViewAtIndex(row, GridView.Beginning)
                            contentY = Math.max(originY, Math.min(originY + Math.max(0, contentHeight - height), contentY - viewportOffset))
                            // The model has resolved this identity even if its
                            // metadata page has not populated the delegate yet.
                            geometryAnchorRow = row
                            geometryAnchorOffset = viewportOffset
                        })
                    }
                    // Keep the user's row across responsive column-count changes,
                    // including changes while Library is hidden behind Develop.
                    // Reflow must not replace that row with the new row's first cell.
                    onWidthChanged: restoreGeometryAnchor()
                    onHeightChanged: restoreGeometryAnchor()
                    onVisibleChanged: { if (visible) restoreGeometryAnchor() }
                    onMovementStarted: { cancelViewportRestore(); cancelGeometryRestore() }
                    onMovementEnded: captureGeometryAnchor()
                    cellWidth: width / Math.max(1,Math.floor(width / 220)); cellHeight: 210
                    cacheBuffer: 420
                    delegate: PhotoCard { width: grid.cellWidth - 12; height: grid.cellHeight - 12 }
                    ScrollBar.vertical: ScrollBar {
                        id: libraryBar
                        objectName: "library.scrollBar"
                        onPressedChanged: {
                            if (pressed) {
                                grid.cancelViewportRestore()
                                grid.cancelGeometryRestore()
                            } else {
                                grid.captureGeometryAnchor()
                            }
                        }
                    }
                }
                Rectangle {
                    objectName: "library.loupe"
                    anchors.fill: parent
                    visible: pane.viewMode === "loupe"
                    color: Theme.panelRaised
                    radius: Theme.radius
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.panelPadding
                        RowLayout {
                            Layout.fillWidth: true
                            ApertoneButton {
                                text: qsTranslate("Apertone", "Fit")
                                onClicked: {
                                    loupeZoom.value = 1
                                    pane.loupeCenterX = 0.5
                                    pane.loupeCenterY = 0.5
                                    if (editor.comparison.fullResolution)
                                        editor.prepareLoupe(pane.loupeID, false)
                                }
                            }
                            ApertoneButton {
                                text: "100%"
                                enabled: !editor.comparison.busy
                                Accessible.name: qsTranslate("Apertone", "Zoom") + " 100%"
                                onClicked: {
                                    loupeZoom.value = 1
                                    editor.prepareLoupe(pane.loupeID, true)
                                }
                            }
                            ApertoneSlider {
                                id: loupeZoom
                                objectName: "library.loupe.zoom"
                                from: 1; to: 4; value: 1
                                enabled: editor.comparison.ready
                                Accessible.name: qsTranslate("Apertone", "Zoom")
                                Layout.maximumWidth: 180
                                Layout.fillWidth: true
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Item {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            ComparePane {
                                objectName: "library.loupe.preview"
                                anchors.fill: parent
                                visible: pane.visible && pane.viewMode === "loupe" && editor.comparison.ready
                                source: visible ? editor.comparison.left : ""
                                title: editor.comparison.names[0] ?? ""
                                zoom: loupeZoom.value
                                pixelScale: editor.comparison.fullResolution
                                centerX: pane.loupeCenterX; centerY: pane.loupeCenterY
                                onPanned: function(x, y) { pane.loupeCenterX = x; pane.loupeCenterY = y }
                            }
                            BusyIndicator {
                                anchors.centerIn: parent
                                running: pane.visible && pane.viewMode === "loupe" && editor.comparison.busy
                                visible: running
                            }
                            Label {
                                anchors.centerIn: parent; width: parent.width
                                visible: editor.comparison.error.length > 0
                                text: qsTranslate("Apertone", editor.comparison.error)
                                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                            }
                        }
                    }
                }
                Label { anchors.centerIn: parent; visible: !library.loading && !library.count; text: qsTranslate("Apertone", "Import photos\nor change the filters"); horizontalAlignment: Text.AlignHCenter; color: Theme.faint; font.pixelSize: 18 }
            }
            GridLayout {
                objectName: "library.metadataActions"
                Layout.minimumWidth: 0; Layout.fillWidth: true
                columns: width < 900 ? 1 : 2; columnSpacing: 12; rowSpacing: 6
                enabled: library.selectedCount > 0 && !library.working
                RowLayout {
                    Layout.minimumWidth: 0; Layout.fillWidth: true; spacing: 6
                    Label { Layout.maximumWidth: 80; elide: Text.ElideRight; text: qsTranslate("Apertone", "Rating"); font.pixelSize: 11 }
                    Repeater { model: 6; ApertoneButton { required property int index; objectName: "library.rate." + index; Accessible.name: qsTranslate("Apertone", "Rating") + " " + index; text: index ? index + "★" : "0"; implicitWidth: 38; onClicked: library.rate(index) } }
                    ApertoneComboBox { objectName: "library.flagChoice"; Layout.minimumWidth: 0; Layout.maximumWidth: 120; model: [qsTranslate("Apertone", "Flag…"), qsTranslate("Apertone", "Pick"), qsTranslate("Apertone", "Reject"), qsTranslate("Apertone", "Unflagged")]; onActivated: { if (currentIndex > 0) library.flag([0,1,-1,0][currentIndex]); currentIndex = 0 } }
                    ApertoneComboBox { objectName: "library.labelChoice"; Layout.minimumWidth: 0; Layout.maximumWidth: 120; model: [qsTranslate("Apertone", "Label…"), qsTranslate("Apertone", "No label"), qsTranslate("Apertone", "Red"), qsTranslate("Apertone", "Yellow"), qsTranslate("Apertone", "Green"), qsTranslate("Apertone", "Blue"), qsTranslate("Apertone", "Purple")]; onActivated: { if (currentIndex > 0) library.label(["","","red","yellow","green","blue","purple"][currentIndex]); currentIndex = 0 } }
                }
                RowLayout {
                    Layout.minimumWidth: 0; Layout.fillWidth: true; spacing: 6
                    ApertoneComboBox { id: targetCollection; Layout.minimumWidth: 0; model: library.collections.filter(c=>!c.smart); textRole: "name"; Layout.maximumWidth: 145 }
                    ApertoneButton { objectName: "libraryAddToCollection"; implicitWidth: 36; text: "+"; enabled: targetCollection.currentIndex >= 0 && targetCollection.count > 0; onClicked: library.collectionMembership(targetCollection.model[targetCollection.currentIndex].id,true); Accessible.name: qsTranslate("Apertone", "Add to collection") }
                    ApertoneButton { objectName: "libraryRemoveFromCollection"; implicitWidth: 36; text: "−"; enabled: !!pane.currentCollection && library.collections.some(c=>c.id===pane.currentCollection&&!c.smart); onClicked: library.collectionMembership(pane.currentCollection,false); Accessible.name: qsTranslate("Apertone", "Remove from collection") }
                }
            }
            Label { text: qsTranslate("Apertone", library.status); font.pixelSize: 10; color: Theme.faint; elide: Text.ElideRight; Layout.fillWidth: true }
        }
    }
}
