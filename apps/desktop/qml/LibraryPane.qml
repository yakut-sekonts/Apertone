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
    property bool rightPanelVisible: true
    property bool panelOnly: false
    property alias exportSlot: exportSlotItem
    property Item compactImportSlot
    readonly property alias importButtonWidth: importButton.implicitWidth
    readonly property int currentGridRow: grid.currentIndex
    readonly property var inspectedPhoto: visible && (leftPanelVisible || rightPanelVisible) && library.selectedCount === 1 &&
                                         !library.selecting && !library.loading ? library.selectedPhotoSummary : ({})

    readonly property string activePreviewSource: {
        if (!visible || (!leftPanelVisible && !rightPanelVisible) || reviewDialogVisible ||
            !library.selectedCount || library.loading || library.selecting)
            return ""
        if (viewMode === "loupe") {
            const review = editor.comparison
            return review.ready && review.ids[0] === loupeID ? review.left : ""
        }
        return inspectedPhoto.thumbnail || ""
    }

    function revealGridRow(row) {
        if (viewMode !== "grid" || row < 0 || row >= grid.count)
            return
        grid.cancelViewportRestore()
        grid.cancelGeometryRestore()
        grid.currentIndex = row
        grid.positionViewAtIndex(row, GridView.Contain)
        grid.captureGeometryAnchor()
    }
    property bool viewToolbarVisible: true
    property string viewMode: "grid"
    property string queuedView: ""
    property string loupeID: ""
    readonly property var loupeCard: grid.currentItem && grid.currentItem.assetID === loupeID ? grid.currentItem : null
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
        function onThumbnailsRefreshed(id) {
            if (pane.visible && pane.viewMode === "loupe" && (!id || id === pane.loupeID))
                editor.prepareLoupe(pane.loupeID, editor.comparison.fullResolution)
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
    function ratePhoto(value) { library.rate(value, pane.viewMode === "loupe" ? pane.loupeID : "") }
    function flagPhoto(value) { library.flag(value, pane.viewMode === "loupe" ? pane.loupeID : "") }
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
        enabled: pane.visible && pane.viewMode === "loupe" && !navMap.activeFocus && !pane.textInputFocused() && !library.working
        onActivated: pane.moveLoupe(-1)
    }
    Shortcut {
        sequence: "Right"
        enabled: pane.visible && pane.viewMode === "loupe" && !navMap.activeFocus && !pane.textInputFocused() && !library.working
        onActivated: pane.moveLoupe(1)
    }
    Shortcut { sequence: "P"; enabled: pane.visible && pane.allowShortcut(); onActivated: pane.flagPhoto(1) }
    Shortcut { sequence: "X"; enabled: pane.visible && pane.allowShortcut(); onActivated: pane.flagPhoto(-1) }
    Shortcut { sequence: "U"; enabled: pane.visible && pane.allowShortcut(); onActivated: pane.flagPhoto(0) }
    Repeater {
        model: 6
        Item { required property int index; Shortcut { sequence: index.toString(); enabled: pane.visible && pane.allowShortcut(); onActivated: pane.ratePhoto(index) } }
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
            Layout.fillWidth: pane.panelOnly
            color: Theme.panel; Layout.preferredWidth: Theme.sidebarWidth; Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent; anchors.margins: Theme.panelPadding; spacing: Theme.panelSpacing
                ScrollView {
                    id: catalogScroll
                    objectName: "library.catalogScroll"
                    Layout.minimumHeight: 0; Layout.minimumWidth: 0
                    Layout.fillHeight: true; Layout.fillWidth: true
                    clip: true; contentWidth: availableWidth
                    rightPadding: ScrollBar.vertical.implicitWidth
                    ColumnLayout {
                        width: catalogScroll.availableWidth
                        height: Math.max(implicitHeight, catalogScroll.availableHeight)
                        spacing: Theme.panelSpacing
                        DevelopSection {
                            objectName: "library.navigator"
                            title: qsTranslate("Apertone", "Navigator")
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(145, width * 0.7, pane.height * 0.27)
                                color: Theme.canvas
                                Image {
                                    id: navImage
                                    objectName: "library.navigatorPreview"
                                    anchors.fill: parent
                                    source: visible ? pane.activePreviewSource : ""
                                    sourceSize: Theme.libraryThumbnailSize
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                }
                                Label {
                                    anchors.centerIn: parent
                                    width: parent.width - 8
                                    visible: navImage.status !== Image.Ready
                                    text: library.loading || library.selecting || navImage.status === Image.Loading ||
                                          (pane.viewMode === "loupe" && editor.comparison.busy)
                                          ? qsTranslate("Apertone", "Loading…")
                                          : library.selectedCount === 0 ? qsTranslate("Apertone", "No photo selected")
                                          : pane.viewMode === "grid" && library.selectedCount > 1
                                            ? qsTranslate("Apertone", "Selected photos: ") + library.selectedCount
                                            : qsTranslate("Apertone", "No preview")
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: Text.AlignHCenter
                                    font.pixelSize: 11
                                    color: Theme.canvasMuted
                                }
                                MouseArea {
                                    id: navMap
                                    objectName: "library.navigatorMap"
                                    anchors.centerIn: navImage
                                    width: navImage.paintedWidth; height: navImage.paintedHeight
                                    visible: navImage.status === Image.Ready
                                    enabled: pane.canRequestView("loupe")
                                    activeFocusOnTab: true
                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTranslate("Apertone", "Navigator")
                                    cursorShape: Qt.CrossCursor
                                    function navigate(x, y) {
                                        if (pane.viewMode !== "loupe" && !pane.requestView("loupe"))
                                            return
                                        if (!editor.comparison.fullResolution && loupeZoom.value === 1)
                                            loupeZoom.value = 2
                                        pane.loupeCenterX = Math.max(0, Math.min(1, x))
                                        pane.loupeCenterY = Math.max(0, Math.min(1, y))
                                    }
                                    onPressed: function(mouse) { navigate(mouse.x / width, mouse.y / height) }
                                    onPositionChanged: function(mouse) {
                                        if (pressed) navigate(mouse.x / width, mouse.y / height)
                                    }
                                    Keys.onSpacePressed: navigate(0.5, 0.5)
                                    Keys.onReturnPressed: navigate(0.5, 0.5)
                                    Keys.onLeftPressed: navigate(pane.loupeCenterX - 0.1, pane.loupeCenterY)
                                    Keys.onRightPressed: navigate(pane.loupeCenterX + 0.1, pane.loupeCenterY)
                                    Keys.onUpPressed: navigate(pane.loupeCenterX, pane.loupeCenterY - 0.1)
                                    Keys.onDownPressed: navigate(pane.loupeCenterX, pane.loupeCenterY + 0.1)
                                    Accessible.onPressAction: navigate(0.5, 0.5)
                                    Rectangle {
                                        objectName: "library.navigatorBounds"
                                        readonly property rect viewport: loupePreview.visibleSourceRect
                                        visible: pane.viewMode === "loupe" && (viewport.width < 1 || viewport.height < 1)
                                        x: viewport.x * parent.width; y: viewport.y * parent.height
                                        width: viewport.width * parent.width; height: viewport.height * parent.height
                                        color: "transparent"; border.width: 1; border.color: "white"
                                    }
                                    Rectangle {
                                        anchors.fill: parent; visible: navMap.activeFocus
                                        color: "transparent"; border.width: 2; border.color: Theme.accent
                                    }
                                }
                            }
                        }
                        Label { text: qsTranslate("Apertone", "CATALOG"); font.pixelSize: 10; color: Theme.muted; font.letterSpacing: 1.2 }
                        ApertoneButton { text: qsTranslate("Apertone", "All photos"); Layout.fillWidth: true; highlighted: !pane.currentCollection; onClicked: { pane.currentCollection = ""; pane.applyFilter() } }
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
                            Layout.fillWidth: true; Layout.fillHeight: true
                            Layout.minimumHeight: collectionRows.count ? Theme.controlHeight * 2 : 0
                            Layout.preferredHeight: 0
                            clip: true; contentWidth: availableWidth
                            Column {
                                width: parent.width; spacing: 4
                                Repeater {
                                    id: collectionRows
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
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                        ApertoneButton { objectName: "library.fingerprintOpen"; text: qsTranslate("Apertone", "Catalog matches"); Layout.fillWidth: true; enabled: !library.working && !library.loading; onClicked: fingerprintDialog.open() }
                        ApertoneButton {
                            objectName: "semantic.open"
                            text: semanticSearch.categorySearch ? qsTranslate("Apertone", "Search by category") : qsTranslate("Apertone", "Search by description")
                            Layout.fillWidth: true
                            onClicked: semanticSearch.open()
                        }
                        ApertoneButton {
                            objectName: "library.importOptions"
                            text: qsTranslate("Apertone", "Import sources")
                            Layout.fillWidth: true
                            onClicked: importOptions.open()
                            Menu {
                                id: importOptions
                                Menu {
                                    id: cards
                                    objectName: "library.cardImport"
                                    title: qsTranslate("Apertone", "Import from card")
                                    enabled: !library.working
                                    property var devices: []
                                    onAboutToShow: devices = library.cardSources()
                                    MenuItem {
                                        visible: !cards.devices.length
                                        height: visible ? implicitHeight : 0
                                        enabled: false
                                        text: qsTranslate("Apertone", "No mounted card with a DCIM folder")
                                    }
                                    Instantiator {
                                        model: cards.devices
                                        delegate: MenuItem {
                                            required property var modelData
                                            text: modelData.name
                                            onTriggered: importDialog.openCard(modelData.url)
                                        }
                                        onObjectAdded: (index, item) => cards.insertItem(index + 1, item)
                                        onObjectRemoved: (index, item) => cards.removeItem(item)
                                    }
                                }
                                MenuItem {
                                    objectName: "library.captureFolder"
                                    text: library.captureFolder ? qsTranslate("Apertone", "Stop watching capture folder") : qsTranslate("Apertone", "Watch capture folder…")
                                    enabled: !!library.captureFolder || !library.working
                                    onTriggered: library.captureFolder ? library.stopCaptureFolder() : captureFolderPicker.open()
                                }
                                MenuItem {
                                    objectName: "library.tetherOpen"
                                    text: qsTranslate("Apertone", "Tethered camera…")
                                    visible: library.tetherAvailable
                                    height: visible ? implicitHeight : 0
                                    enabled: !library.working
                                    onTriggered: cameraDialog.open()
                                }
                                MenuItem {
                                    objectName: "library.lightroomButton"
                                    text: qsTranslate("Apertone", "Import Lightroom catalog…")
                                    enabled: !library.working
                                    onTriggered: lightroomFile.open()
                                }
                            }
                        }
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.panelSpacing
                    Item {
                        id: sidebarImportSlot
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        implicitWidth: importButton.implicitWidth
                        implicitHeight: Theme.controlHeight
                        ApertoneButton {
                            id: importButton
                            objectName: "library.importButton"
                            parent: pane.compactImportSlot || sidebarImportSlot
                            anchors.fill: parent
                            text: qsTranslate("Apertone", "Import")
                            enabled: !library.working
                            onClicked: pane.requestImport()
                        }
                    }
                    Item {
                        id: exportSlotItem
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        implicitWidth: importButton.implicitWidth
                        implicitHeight: Theme.controlHeight
                    }
                }
            }
        }
        ColumnLayout {
            visible: !pane.panelOnly
            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.fillHeight: true; Layout.margins: Theme.panelPadding; spacing: Theme.panelSpacing
            GridLayout {
                id: filterBar
                visible: pane.viewMode === "grid"
                objectName: "library.filterBar"
                Layout.minimumWidth: 0
                Layout.fillWidth: true
                columns: width < 590 ? 3 : 4
                columnSpacing: Theme.panelSpacing
                rowSpacing: Theme.panelSpacing
                ApertoneTextField { id: search; objectName: "library.search"; Layout.columnSpan: filterBar.columns === 3 ? 3 : 1; Layout.minimumWidth: 0; placeholderTextColor: Theme.muted; placeholderText: qsTranslate("Apertone", "Search: name, path, camera, lens"); selectByMouse: true; maximumLength: 1024; Layout.fillWidth: true; onTextChanged: pane.applyFilter() }
                ApertoneComboBox { id: ratingFilter; objectName: "library.ratingFilter"; Layout.minimumWidth: 0; Layout.fillWidth: filterBar.columns === 3; Layout.preferredWidth: 82; model: [qsTranslate("Apertone", "All ★"), "≥ 1 ★", "≥ 2 ★", "≥ 3 ★", "≥ 4 ★", "5 ★"]; onActivated: pane.applyFilter() }
                ApertoneComboBox { id: flagFilter; objectName: "libraryFlagFilter"; Layout.minimumWidth: 0; Layout.fillWidth: filterBar.columns === 3; Layout.preferredWidth: 114; ToolTip.visible: hovered; ToolTip.text: currentText; model: [qsTranslate("Apertone", "All flags"), qsTranslate("Apertone", "Reject"), qsTranslate("Apertone", "Unflagged"), qsTranslate("Apertone", "Pick")]; onActivated: pane.applyFilter() }
                ApertoneComboBox { id: labelFilter; objectName: "libraryLabelFilter"; Layout.minimumWidth: 0; Layout.fillWidth: filterBar.columns === 3; Layout.preferredWidth: 114; ToolTip.visible: hovered; ToolTip.text: currentText; model: [qsTranslate("Apertone", "All labels"), qsTranslate("Apertone", "No label"), qsTranslate("Apertone", "Red"), qsTranslate("Apertone", "Yellow"), qsTranslate("Apertone", "Green"), qsTranslate("Apertone", "Blue"), qsTranslate("Apertone", "Purple")]; onActivated: pane.applyFilter() }
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
                visible: library.importReport.length > 0 || library.duplicateReview.length > 0
                ApertoneButton { objectName: "library.importReportButton"; visible: library.importReport.length > 0; text: qsTranslate("Apertone", "Import errors"); onClicked: reportDialog.open() }
                ApertoneButton { objectName: "library.duplicateReviewButton"; visible: library.duplicateReview.length > 0; text: qsTranslate("Apertone", "Duplicates") + " · " + library.duplicateReview.length; onClicked: duplicateReviewDialog.open() }
                Item { Layout.fillWidth: true }
            }
            RowLayout {
                visible: pane.viewMode === "grid"
                Layout.minimumWidth: 0; Layout.fillWidth: true
                Label { text: library.loading ? qsTranslate("Apertone", "Loading…") : library.count + qsTranslate("Apertone", " photos · selected ") + library.selectedCount; elide: Text.ElideRight; Layout.minimumWidth: 0; color: Theme.muted; Layout.fillWidth: true }
                ApertoneButton { visible: library.viewingStack; text: qsTranslate("Apertone", "Exit stack"); enabled: !library.working; onClicked: library.viewStack("") }
                ApertoneButton { objectName: "library.selectAll"; text: qsTranslate("Apertone", "Select all"); enabled: library.count > 0; onClicked: library.selectAll() }
                ApertoneButton { objectName: "library.clearSelection"; text: qsTranslate("Apertone", "Clear selection"); enabled: library.selectedCount > 0; onClicked: library.clearSelection() }
            }
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
                    color: Theme.canvas
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.panelPadding
                        Item {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            ComparePane {
                                id: loupePreview
                                objectName: "library.loupe.preview"
                                anchors.fill: parent
                                visible: pane.visible && pane.viewMode === "loupe" && editor.comparison.ready
                                source: visible ? editor.comparison.left : ""
                                title: ""
                                zoom: editor.comparison.fullResolution ? loupeZoom.value / Screen.devicePixelRatio : loupeZoom.value
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
                                color: Theme.canvasText
                                text: qsTranslate("Apertone", editor.comparison.error)
                                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                            }
                        }
                        RowLayout {
                            visible: pane.viewToolbarVisible
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
                                Layout.minimumWidth: 36; Layout.maximumWidth: 130
                                Layout.fillWidth: true
                            }
                            RowLayout {
                                objectName: "library.loupe.rating"
                                spacing: 0
                                enabled: !!pane.loupeCard && pane.loupeCard.selected && !library.working
                                Repeater {
                                    model: 5
                                    ApertoneButton {
                                        required property int index
                                        objectName: "library.loupe.star." + (index + 1)
                                        readonly property int currentRating: pane.loupeCard?.rating ?? 0
                                        text: currentRating > index ? "★" : "☆"
                                        font.pixelSize: 15
                                        flat: true
                                        contentItem: Text {
                                            text: parent.text; font: parent.font
                                            color: parent.hovered || parent.visualFocus ? "white" : Theme.canvasText
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            radius: Theme.radius
                                            color: parent.hovered || parent.down ? Theme.canvasRaised : "transparent"
                                            border.width: parent.visualFocus ? 1 : 0; border.color: Theme.accent
                                        }
                                        leftPadding: 0; rightPadding: 0
                                        Layout.preferredWidth: 18
                                        Accessible.name: qsTranslate("Apertone", "Rating") + " " + (index + 1)
                                        ToolTip.visible: hovered || visualFocus
                                        ToolTip.text: Accessible.name
                                        onClicked: pane.ratePhoto(currentRating === index + 1 ? 0 : index + 1)
                                    }
                                }
                            }
                            ApertoneComboBox {
                                objectName: "library.loupe.flag"
                                Layout.preferredWidth: 96; Layout.minimumWidth: 58
                                enabled: !!pane.loupeCard && pane.loupeCard.selected && !library.working
                                Accessible.name: qsTranslate("Apertone", "Flag…")
                                model: [qsTranslate("Apertone", "Unflagged"), qsTranslate("Apertone", "Pick"), qsTranslate("Apertone", "Reject")]
                                currentIndex: pane.loupeCard?.flagValue === 1 ? 1 : pane.loupeCard?.flagValue === -1 ? 2 : 0
                                ToolTip.visible: hovered || visualFocus
                                ToolTip.text: currentText
                                onActivated: function(index) { pane.flagPhoto([0, 1, -1][index]) }
                            }
                            Label {
                                visible: parent.width >= 520
                                text: editor.comparison.names[0] ?? ""
                                textFormat: Text.PlainText
                                color: Theme.canvasMuted; font.pixelSize: 11; elide: Text.ElideMiddle
                                horizontalAlignment: Text.AlignRight
                                Layout.minimumWidth: 0; Layout.fillWidth: true
                            }
                        }
                    }
                }
                Label { anchors.centerIn: parent; visible: !library.loading && !library.count; text: qsTranslate("Apertone", "Import photos\nor change the filters"); horizontalAlignment: Text.AlignHCenter; color: Theme.faint; font.pixelSize: 18 }
            }
            GridLayout {
                objectName: "library.viewToolbar"
                visible: pane.viewToolbarVisible
                Layout.fillWidth: true; Layout.minimumWidth: 0
                columns: width >= viewButtons.implicitWidth + viewOptions.implicitWidth + columnSpacing ? 2 : 1
                rowSpacing: 2; columnSpacing: Theme.panelSpacing
                RowLayout {
                    id: viewButtons
                    Layout.alignment: Qt.AlignLeft
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
                }
                RowLayout {
                    id: viewOptions
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    ApertoneComboBox {
                        objectName: "library.sortOrder"
                        visible: pane.viewMode !== "loupe"
                        Layout.preferredWidth: 140
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
                }
            }
            Label { visible: text.length > 0; text: qsTranslate("Apertone", library.status); font.pixelSize: 10; color: Theme.faint; elide: Text.ElideRight; Layout.fillWidth: true }
        }
        Rectangle {
            objectName: "library.rightPanel"
            visible: pane.rightPanelVisible
            color: Theme.panel
            Layout.fillWidth: pane.panelOnly
            Layout.preferredWidth: Math.min(Theme.inspectorWidth, pane.width * (pane.leftPanelVisible ? 0.27 : 0.4))
            Layout.fillHeight: true
            ScrollView {
                id: inspectorScroll
                objectName: "library.inspectorScroll"
                anchors.fill: parent
                anchors.margins: Theme.panelPadding
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: inspectorScroll.availableWidth
                    spacing: Theme.panelSpacing
                    DevelopSection {
                        objectName: "library.histogramSection"
                        title: qsTranslate("Apertone", "Histogram")
                        LibraryHistogram {
                            objectName: "library.histogram"
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.max(54, Math.min(implicitHeight, pane.height * 0.17))
                            source: visible ? pane.activePreviewSource : ""
                        }
                    }
                    DevelopSection {
                        objectName: "library.quickDevelop"
                        title: qsTranslate("Apertone", "Quick Develop")
                        LibraryPresetPicker { id: quickPresets }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 3
                            columnSpacing: Theme.panelSpacing
                            rowSpacing: Theme.panelSpacing
                            enabled: quickPresets.enabled
                            Label {
                                visible: library.selectedCount > 1
                                Layout.columnSpan: 3; Layout.fillWidth: true
                                text: qsTranslate("Apertone", "Selected photos: ") + library.selectedCount
                                color: Theme.muted; font.pixelSize: 11; wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                text: qsTranslate("Apertone", "Exposure")
                                color: Theme.text; wrapMode: Text.Wrap; font.pixelSize: 11
                            }
                            Repeater {
                                model: ["−⅓", "+⅓"]
                                ApertoneButton {
                                    required property int index
                                    required property string modelData
                                    objectName: index ? "library.quickExposureUp" : "library.quickExposureDown"
                                    text: modelData
                                    Accessible.name: qsTranslate("Apertone", "Exposure") + " " + modelData + " EV"
                                    ToolTip.visible: hovered || visualFocus
                                    ToolTip.text: Accessible.name + " · " + qsTranslate("Apertone", "Apply to selection")
                                    onClicked: batch.adjustExposure(index === 1)
                                }
                            }
                            Label {
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                text: qsTranslate("Apertone", "White balance")
                                color: Theme.text; wrapMode: Text.Wrap; font.pixelSize: 11
                            }
                            ApertoneButton {
                                objectName: "library.quickAsShot"
                                Layout.columnSpan: 2; Layout.fillWidth: true; Layout.minimumWidth: 0
                                text: qsTranslate("Apertone", "As Shot")
                                Accessible.name: qsTranslate("Apertone", "White balance") + " · " + text
                                ToolTip.visible: hovered || visualFocus
                                ToolTip.text: Accessible.name + " · " + qsTranslate("Apertone", "Apply to selection")
                                onClicked: batch.asShotWhiteBalance()
                            }
                        }
                        ApertoneButton {
                            objectName: "library.openInDevelop"
                            text: qsTranslate("Apertone", "Open in Develop")
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            enabled: pane.canOpenInDevelop && (library.selectedCount === 1 || pane.viewMode === "loupe")
                            onClicked: pane.openSelectedInDevelop()
                        }
                    }
                    DevelopSection {
                        objectName: "library.metadataSection"
                        title: qsTranslate("Apertone", "Metadata")
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Label {
                                objectName: "library.inspectorFilename"
                                text: pane.inspectedPhoto.filename || (library.selecting || library.loading ? qsTranslate("Apertone", "Loading…") :
                                      library.selectedCount ? qsTranslate("Apertone", "Selected photos: ") + library.selectedCount :
                                                              qsTranslate("Apertone", "No photo selected"))
                                textFormat: Text.PlainText
                                color: Theme.text
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                            }
                            Label {
                                visible: text.length > 0
                                text: pane.inspectedPhoto.camera || ""
                                textFormat: Text.PlainText
                                color: Theme.muted
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                            }
                        }
                        ColumnLayout {
                            objectName: "library.metadataActions"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: Theme.panelSpacing
                            enabled: library.selectedCount > 0 && !library.working && !library.selecting
                            Label { text: qsTranslate("Apertone", "Rating"); color: Theme.muted; font.pixelSize: 11 }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Repeater {
                                    model: 6
                                    ApertoneButton {
                                        required property int index
                                        objectName: "library.rate." + index
                                        Accessible.name: qsTranslate("Apertone", "Rating") + " " + index
                                        text: index ? index + "★" : "0"
                                        leftPadding: 2; rightPadding: 2
                                        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 30
                                        highlighted: pane.inspectedPhoto.rating === index
                                        onClicked: library.rate(index)
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                ApertoneComboBox {
                                    objectName: "library.flagChoice"
                                    Layout.minimumWidth: 0; Layout.fillWidth: true; Layout.preferredWidth: 100
                                    model: [qsTranslate("Apertone", "Flag…"), qsTranslate("Apertone", "Pick"), qsTranslate("Apertone", "Reject"), qsTranslate("Apertone", "Unflagged")]
                                    displayText: pane.inspectedPhoto.flag === undefined ? model[0] : model[pane.inspectedPhoto.flag === 1 ? 1 : pane.inspectedPhoto.flag === -1 ? 2 : 3]
                                    onActivated: {
                                        if (currentIndex > 0) library.flag([0, 1, -1, 0][currentIndex])
                                        currentIndex = 0
                                    }
                                }
                                ApertoneComboBox {
                                    objectName: "library.labelChoice"
                                    Layout.minimumWidth: 0; Layout.fillWidth: true; Layout.preferredWidth: 100
                                    readonly property var values: ["", "red", "yellow", "green", "blue", "purple"]
                                    model: [qsTranslate("Apertone", "Label…"), qsTranslate("Apertone", "No label"), qsTranslate("Apertone", "Red"), qsTranslate("Apertone", "Yellow"), qsTranslate("Apertone", "Green"), qsTranslate("Apertone", "Blue"), qsTranslate("Apertone", "Purple")]
                                    displayText: {
                                        const color = pane.inspectedPhoto.label
                                        if (color === undefined) return model[0]
                                        const index = values.indexOf(color.toLowerCase())
                                        return index < 0 ? color : model[index + 1]
                                    }
                                    onActivated: {
                                        if (currentIndex > 0) library.label(values[currentIndex - 1])
                                        currentIndex = 0
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                ApertoneComboBox {
                                    id: targetCollection
                                    Layout.minimumWidth: 0; Layout.fillWidth: true
                                    model: library.collections.filter(collection => !collection.smart)
                                    textRole: "name"
                                    enabled: count > 0
                                    Accessible.name: qsTranslate("Apertone", "Add to collection")
                                    displayText: currentIndex >= 0 ? currentText : Accessible.name
                                }
                                ApertoneButton { objectName: "libraryAddToCollection"; implicitWidth: 36; text: "+"; enabled: targetCollection.currentIndex >= 0 && targetCollection.count > 0; onClicked: library.collectionMembership(targetCollection.model[targetCollection.currentIndex].id,true); Accessible.name: qsTranslate("Apertone", "Add to collection") }
                                ApertoneButton { objectName: "libraryRemoveFromCollection"; implicitWidth: 36; text: "−"; enabled: !!pane.currentCollection && library.collections.some(c=>c.id===pane.currentCollection&&!c.smart); onClicked: library.collectionMembership(pane.currentCollection,false); Accessible.name: qsTranslate("Apertone", "Remove from collection") }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            ApertoneButton {
                                text: qsTranslate("Apertone", "Photo metadata")
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                enabled: library.selectedCount === 1 && !library.working && !library.selecting
                                onClicked: photoMetadata.showPhoto(library.selectedAssetIDs()[0])
                            }
                            ApertoneButton {
                                objectName: "library.keywordTree"
                                text: qsTranslate("Apertone", "Keywords…")
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                enabled: !library.working
                                onClicked: keywordEditor.open()
                            }
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
                                MenuItem {
                                    text: qsTranslate("Apertone", "Collapse stacks")
                                    visible: !library.viewingStack
                                    height: visible ? implicitHeight : 0
                                    enabled: !library.working
                                    checkable: true
                                    checked: library.collapseStacks
                                    onTriggered: library.collapseStacks = checked
                                }
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
                    }
                    DevelopSection {
                        objectName: "library.batchSection"
                        title: qsTranslate("Apertone", "Batch processing")
                        BatchSettings {}
                    }
                    DevelopSection {
                        objectName: "intelligence.controls"
                        title: qsTranslate("Apertone", "INTELLIGENT")
                        expanded: false
                        ApertoneComboBox {
                            id: suggestionFilter; objectName: "intelligence.suggestionFilter"
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            model: [qsTranslate("Apertone", "All photos"), qsTranslate("Apertone", "Pick"), qsTranslate("Apertone", "Review"), qsTranslate("Apertone", "Reject")]
                            onActivated: pane.applyIntelligentFilter()
                        }
                        ApertoneComboBox {
                            id: faceGroupFilter; objectName: "intelligence.faceGroupFilter"
                            Layout.fillWidth: true; Layout.minimumWidth: 0; model: pane.intelligenceGroups; textRole: "displayName"
                            onActivated: { pane.intelligentFaceGroupID = currentIndex > 0 ? model[currentIndex].id : ""; pane.applyIntelligentFilter() }
                        }
                        ApertoneButton {
                            Layout.fillWidth: true
                            objectName: "intelligence.renameFaceGroup"; text: qsTranslate("Apertone", "Name")
                            visible: faceGroupFilter.currentIndex > 0; enabled: !library.working
                            onClicked: faceGroupDialog.open()
                        }
                        ApertoneCheckBox {
                            id: bestOfBurst; objectName: "intelligence.bestOfBurst"
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            text: qsTranslate("Apertone", "Best of burst"); font.pixelSize: 11
                            onToggled: pane.applyIntelligentFilter()
                        }
                        ApertoneCheckBox {
                            objectName: "intelligence.autoAnalyze"
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            text: qsTranslate("Apertone", "Analyze new imports locally"); font.pixelSize: 11
                            checked: library.cullingAutoEnabled
                            onClicked: library.cullingAutoEnabled = checked
                        }
                        Label {
                            visible: library.cullingSummary.analyzed > 0
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            wrapMode: Text.Wrap
                            text: library.cullingSummary.picks + " " + qsTranslate("Apertone", "picks") + " · " +
                                  library.cullingSummary.faceGroups + " " + qsTranslate("Apertone", "face groups")
                            color: Theme.faint; font.pixelSize: 10
                        }
                        ApertoneButton {
                            Layout.fillWidth: true
                            objectName: "intelligence.apply"; text: library.selectedCount > 0 ? qsTranslate("Apertone", "Apply to selection") : qsTranslate("Apertone", "Apply suggestions")
                            enabled: !library.working && library.cullingSummary.analyzed > 0 && (library.selectedCount > 0 || library.count > 0)
                            onClicked: applySuggestionsDialog.open()
                        }
                        ApertoneButton {
                            Layout.fillWidth: true
                            objectName: "intelligence.analyze"; text: library.selectedCount > 0 ? qsTranslate("Apertone", "Analyze selection") : qsTranslate("Apertone", "Analyze library")
                            enabled: !library.working && library.count > 0
                            onClicked: library.analyzeLibrary()
                        }
                        ApertoneButton {
                            Layout.fillWidth: true
                            objectName: "intelligence.clear"; text: qsTranslate("Apertone", "Clear analysis")
                            enabled: !library.working && library.cullingSummary.analyzed > 0
                            onClicked: clearAnalysisDialog.open()
                        }
                    }
                }
            }
        }
    }
}
