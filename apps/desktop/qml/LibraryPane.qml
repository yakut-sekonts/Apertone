import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Rectangle {
    id: pane
    color: Theme.window
    property string currentCollection: ""
    property string editCollectionID: ""
    property string intelligentPersonID: ""
    property var intelligencePeople: [{"id":"", "displayName":qsTranslate("Apertone", "All face groups"), "name":"", "count":0}].concat(library.people)
    onIntelligencePeopleChanged: Qt.callLater(function() {
        const index = intelligencePeople.findIndex(p => p.id === pane.intelligentPersonID)
        if (index < 0) pane.intelligentPersonID = ""
        peopleFilter.currentIndex = Math.max(0, index)
    })
    CompareDialog { id:compareDialog }
    SmartCollectionDialog { id: smartCollection }
    SemanticSearchDialog { id: semanticSearch }
    PhotoMetadataDialog { id: photoMetadata }
    Dialog {
        id: personDialog; objectName: "intelligence.personDialog"
        title: qsTranslate("Apertone", "Name face group")
        modal: true; anchors.centerIn: parent; width: 360; implicitWidth: 360
        standardButtons: Dialog.Ok | Dialog.Cancel
        ApertoneTextField {
            id: personName; objectName: "intelligence.personName"
            width: 300; maximumLength: 128; selectByMouse: true
            placeholderText: qsTranslate("Apertone", "Group name")
        }
        onOpened: {
            const person = pane.intelligencePeople.find(p => p.id === pane.intelligentPersonID)
            personName.text = person ? person.name : ""
            personName.forceActiveFocus()
        }
        onAccepted: {
            if (pane.intelligentPersonID.length > 0)
                library.renamePerson(pane.intelligentPersonID, personName.text)
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
        onAccepted: { pane.intelligentPersonID = ""; library.clearCullingIndex() }
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
            Slider {
                id: panoramaFOV; from: 10; to: 140; value: 60; stepSize: 1
                enabled: panoramaProjection.currentIndex !== 0
                Layout.fillWidth: true; Accessible.name: qsTranslate("Apertone", "Frame field of view")
            }
            CheckBox { id: panoramaWarp; text: qsTranslate("Apertone", "Boundary warp · stretch edges to reduce cropping"); Layout.fillWidth: true }
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
            CheckBox {
                id: hdrAlignment; objectName: "merge.alignment"
                text: qsTranslate("Apertone", "Align frames"); checked: true
                Layout.fillWidth: true
            }
            Slider { id: hdrDeghost; objectName: "merge.deghost"; from: 0; to: 100; value: 50; Layout.fillWidth: true; Accessible.name: qsTranslate("Apertone", "Deghosting") }
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
        function onReloadStarted(preserveViewport) { grid.captureViewport(preserveViewport) }
        function onViewportRestored(row) { grid.restoreViewport(row) }
    }
    function applyFilter() { library.filter(search.text, ratingFilter.currentIndex, flagFilter.currentIndex - 2, ["any","","red","yellow","green","blue","purple"][labelFilter.currentIndex], currentCollection) }
    function applyIntelligentFilter() {
        library.setIntelligentFilter([-2, 1, 0, -1][suggestionFilter.currentIndex], pane.intelligentPersonID, bestOfBurst.checked)
    }
    function allowShortcut() { return library.selectedCount > 0 && !library.working && !(Window.window.activeFocusItem && typeof Window.window.activeFocusItem.cursorPosition === "number") }
    Shortcut { sequence: "P"; enabled: pane.visible && pane.allowShortcut(); onActivated: library.flag(1) }
    Shortcut { sequence: "X"; enabled: pane.visible && pane.allowShortcut(); onActivated: library.flag(-1) }
    Shortcut { sequence: "U"; enabled: pane.visible && pane.allowShortcut(); onActivated: library.flag(0) }
    Repeater {
        model: 6
        Item { required property int index; Shortcut { sequence: index.toString(); enabled: pane.visible && pane.allowShortcut(); onActivated: library.rate(index) } }
    }
    Shortcut { sequences: [StandardKey.SelectAll]; enabled: pane.visible && !(search.activeFocus); onActivated: library.selectAll() }
    ImportDialog { id: importDialog }
    Dialog {
        id: collectionDialog; objectName: "library.collectionDialog"; width: 360; implicitWidth: 360; title: pane.editCollectionID ? qsTranslate("Apertone", "Rename collection") : qsTranslate("Apertone", "New collection"); modal: true; anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: { standardButton(Dialog.Ok).objectName = "library.collectionConfirm"; standardButton(Dialog.Cancel).objectName = "library.collectionCancel" }
        ApertoneTextField { id: collectionName; objectName: "library.collectionName"; width: 300; placeholderText: qsTranslate("Apertone", "Collection name"); maximumLength: 128; selectByMouse: true }
        onAccepted: { if (pane.editCollectionID) library.renameCollection(pane.editCollectionID, collectionName.text); else library.createCollection(collectionName.text) }
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
    RowLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            color: Theme.panel; Layout.preferredWidth: Theme.sidebarWidth; Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent; anchors.margins: Theme.panelPadding; spacing: 8
                Label { text: qsTranslate("Apertone", "LIBRARY"); font.pixelSize: 10; color: Theme.faint; font.letterSpacing: 1.5 }
                ApertoneButton { objectName: "library.importButton"; text: qsTranslate("Apertone", "+  Import"); Layout.fillWidth: true; enabled: !library.working; onClicked: importDialog.open() }
                ApertoneButton { text: qsTranslate("Apertone", "All photos"); Layout.fillWidth: true; highlighted: !pane.currentCollection; onClicked: { pane.currentCollection = ""; pane.applyFilter() } }
                ApertoneButton { objectName: "semantic.open"; text: qsTranslate("Apertone", "Search by description"); Layout.fillWidth: true; onClicked: semanticSearch.open() }
                ApertoneButton {
                    objectName: "library.photoActions"
                    text: qsTranslate("Apertone", "Photo actions")
                    Layout.fillWidth: true
                    onClicked: photoActions.open()
                    Menu {
                        id: photoActions
                        MenuItem { objectName: "library.photoMetadata"; text: qsTranslate("Apertone", "Photo metadata"); enabled: library.selectedCount === 1 && !library.working; onTriggered: photoMetadata.showPhoto(library.selectedAssetIDs()[0]) }
                        MenuItem { text: qsTranslate("Apertone", "Compare Select / Candidate"); enabled: library.selectedCount === 2 && !library.working && !editor.busy; onTriggered: {editor.prepareCompare(library.selectedAssetIDs());compareDialog.open()} }
                        MenuItem { objectName: "library.hdr"; text: qsTranslate("Apertone", "HDR merge"); enabled: library.selectedCount >= 2 && library.selectedCount <= 9 && !library.working && !editor.aiBusy && !editor.busy; onTriggered: hdrDialog.open() }
                        MenuItem { objectName: "library.panorama"; text: qsTranslate("Apertone", "Panorama merge"); enabled: library.selectedCount >= 2 && library.selectedCount <= 24 && !library.working && !editor.aiBusy && !editor.busy; onTriggered: panoramaDialog.open() }
                    }
                }
                Label { text: qsTranslate("Apertone", "COLLECTIONS"); font.pixelSize: 10; color: Theme.faint; Layout.topMargin: 16 }
                ScrollView {
                    objectName: "library.collectionsScroll"
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
                    Column {
                        width: parent.width; spacing: 4
                        Repeater {
                            model: library.collections
                            ApertoneButton { required property var modelData; objectName: "library.collection." + modelData.id; width: parent.width; text: modelData.name + " · " + modelData.count; highlighted: pane.currentCollection === modelData.id; onClicked: { pane.currentCollection = modelData.id; pane.applyFilter() } }
                        }
                    }
                }
                ApertoneButton {text:qsTranslate("Apertone","+ Smart Collection");Layout.fillWidth:true;enabled:!library.working;onClicked:smartCollection.create()}
                ApertoneButton {
                    text:qsTranslate("Apertone","Edit rules");Layout.fillWidth:true;enabled:!library.working
                    visible:!!pane.currentCollection && library.collections.some(c=>c.id===pane.currentCollection&&c.smart)
                    onClicked:{const c=library.collections.find(c=>c.id===pane.currentCollection);if(c)library.editSmartCollection(c.id,c.name)}
                }
                ApertoneButton { objectName: "library.newCollection"; text: qsTranslate("Apertone", "+ Collection"); Layout.fillWidth: true; enabled: !library.working; onClicked: { pane.editCollectionID = ""; collectionName.text = ""; collectionDialog.open() } }
                ApertoneButton { objectName: "library.renameCollection"; text: qsTranslate("Apertone", "Rename"); visible: !!pane.currentCollection; enabled: !library.working; Layout.fillWidth: true; onClicked: { pane.editCollectionID = pane.currentCollection; const item = library.collections.find(c => c.id === pane.currentCollection); collectionName.text = item ? item.name : ""; collectionDialog.open() } }
                ApertoneButton { objectName: "library.deleteCollection"; text: qsTranslate("Apertone", "Delete collection"); visible: !!pane.currentCollection; enabled: !library.working; Layout.fillWidth: true; onClicked: deleteDialog.open() }
                Label { text: qsTranslate("Apertone", "Double-click → Develop\n⌘ / Shift → select\n0–5 · P / X / U"); color: Theme.faint; font.pixelSize: 10; Layout.topMargin: 8 }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.fillHeight: true; Layout.margins: Theme.panelPadding; spacing: 8
            RowLayout {
                Layout.minimumWidth: 0; Layout.fillWidth: true
                ApertoneTextField { id: search; objectName: "library.search"; Layout.minimumWidth: 0; placeholderTextColor: Theme.muted; placeholderText: qsTranslate("Apertone", "Search: name, path, camera, lens"); selectByMouse: true; maximumLength: 1024; Layout.fillWidth: true; onTextChanged: pane.applyFilter() }
                ApertoneComboBox { id: ratingFilter; objectName: "library.ratingFilter"; Layout.minimumWidth: 0; Layout.maximumWidth: 110; model: [qsTranslate("Apertone", "All ★"), "≥ 1 ★", "≥ 2 ★", "≥ 3 ★", "≥ 4 ★", "5 ★"]; onActivated: pane.applyFilter() }
                ApertoneComboBox { id: flagFilter; objectName: "libraryFlagFilter"; Layout.minimumWidth: 0; Layout.maximumWidth: 140; ToolTip.visible: hovered; ToolTip.text: currentText; model: [qsTranslate("Apertone", "All flags"), qsTranslate("Apertone", "Reject"), qsTranslate("Apertone", "Unflagged"), qsTranslate("Apertone", "Pick")]; onActivated: pane.applyFilter() }
                ApertoneComboBox { id: labelFilter; objectName: "libraryLabelFilter"; Layout.minimumWidth: 0; Layout.maximumWidth: 140; ToolTip.visible: hovered; ToolTip.text: currentText; model: [qsTranslate("Apertone", "All labels"), qsTranslate("Apertone", "No label"), qsTranslate("Apertone", "Red"), qsTranslate("Apertone", "Yellow"), qsTranslate("Apertone", "Green"), qsTranslate("Apertone", "Blue"), qsTranslate("Apertone", "Purple")]; onActivated: pane.applyFilter() }
            }
            GridLayout {
                objectName: "intelligence.controls"
                Layout.minimumWidth: 0; Layout.fillWidth: true; columnSpacing: 6; rowSpacing: 5
                columns: width < 880 ? 4 : 9
                Label { text: qsTranslate("Apertone", "INTELLIGENT"); font.pixelSize: 9; color: Theme.faint; font.letterSpacing: 1.2 }
                ApertoneComboBox {
                    id: suggestionFilter; objectName: "intelligence.suggestionFilter"
                    Layout.maximumWidth: 135
                    model: [qsTranslate("Apertone", "All photos"), qsTranslate("Apertone", "Pick"), qsTranslate("Apertone", "Review"), qsTranslate("Apertone", "Reject")]
                    onActivated: pane.applyIntelligentFilter()
                }
                ApertoneComboBox {
                    id: peopleFilter; objectName: "intelligence.peopleFilter"
                    Layout.maximumWidth: 170; model: pane.intelligencePeople; textRole: "displayName"
                    onActivated: { pane.intelligentPersonID = currentIndex > 0 ? model[currentIndex].id : ""; pane.applyIntelligentFilter() }
                }
                ApertoneButton {
                    objectName: "intelligence.renamePerson"; text: qsTranslate("Apertone", "Name")
                    visible: peopleFilter.currentIndex > 0; enabled: !library.working
                    onClicked: personDialog.open()
                }
                CheckBox {
                    id: bestOfBurst; objectName: "intelligence.bestOfBurst"
                    text: qsTranslate("Apertone", "Best of burst"); font.pixelSize: 11
                    onToggled: pane.applyIntelligentFilter()
                }
                CheckBox {
                    objectName: "intelligence.autoAnalyze"
                    text: qsTranslate("Apertone", "Analyze new imports locally"); font.pixelSize: 11
                    checked: library.cullingAutoEnabled
                    onClicked: library.cullingAutoEnabled = checked
                }
                Label {
                    visible: library.cullingSummary.analyzed > 0
                    text: library.cullingSummary.picks + " " + qsTranslate("Apertone", "picks") + " · " +
                          library.cullingSummary.people + " " + qsTranslate("Apertone", "face groups")
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
                visible: library.importing; Layout.fillWidth: true
                ProgressBar { value: library.progress; indeterminate: library.progress === 0; Layout.fillWidth: true }
                ApertoneButton { objectName: "library.cancelImport"; text: qsTranslate("Apertone", "Cancel import"); onClicked: library.cancelImport() }
            }
            Label { objectName: "library.error"; visible: library.error.length > 0; text: qsTranslate("Apertone", library.error); color: Theme.negative; wrapMode: Text.Wrap; Layout.fillWidth: true }
            RowLayout {
                Layout.minimumWidth: 0; Layout.fillWidth: true
                ApertoneButton { objectName: "library.importReportButton"; visible: library.importReport.length > 0; text: qsTranslate("Apertone", "Import errors"); onClicked: reportDialog.open() }
                Label { text: library.loading ? qsTranslate("Apertone", "Loading…") : library.count + qsTranslate("Apertone", " photos · selected ") + library.selectedCount; color: Theme.muted; Layout.fillWidth: true }
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
            Item {
                Layout.fillWidth: true; Layout.fillHeight: true
                GridView {
                    id: grid; objectName: "library.grid"; anchors.fill: parent; clip: true; model: library
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
