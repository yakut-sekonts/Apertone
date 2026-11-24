import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    objectName: "main.window"
    visible: true
    // Screen geometry is in logical pixels, including when display scaling changes.
    readonly property int availableWidth: Screen.desktopAvailableWidth || 1320
    readonly property int availableHeight: Math.max(1, (Screen.desktopAvailableHeight || 872) - 32)
    minimumWidth: Math.min(Qt.platform.os === "windows" ? 480 : 1000, availableWidth)
    minimumHeight: Math.min(Qt.platform.os === "windows" ? 300 : 640, Math.floor((Screen.height || 800) * 0.8), availableHeight)
    Component.onCompleted: {
        width = Math.min(1320, availableWidth)
        height = Math.min(840, availableHeight)
    }
    title: "Apertone " + Qt.application.version + (editor.hasPhoto ? " — " + editor.filename : "")
    color: Theme.window
    palette.window: Theme.window
    palette.base: Theme.panel
    palette.alternateBase: Theme.panelRaised
    palette.text: Theme.text
    palette.windowText: Theme.text
    palette.placeholderText: Theme.faint
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.dark ? Theme.text : "#ffffff"
    palette.button: Theme.control
    palette.mid: Theme.divider
    palette.dark: Theme.divider
    palette.light: Theme.panelRaised
    font.family: Qt.platform.os === "windows" ? "Segoe UI" : "Avenir Next"
    font.pixelSize: 12
    Binding { target: Theme; property: "dark"; value: appearance.dark }
    onLibraryModeChanged: {if(libraryMode) { editor.cancelWhiteBalance(); editor.cancelCreative(); editor.cancelAI(); editor.retouchMode="off"; editor.localMode=false; editor.cropMode=false }}
    property bool libraryMode: true
    property string beforeAfterPhoto: ""
    readonly property bool beforeAfterActive: beforeAfterPhoto !== ""
    readonly property bool comparisonAvailable: !libraryMode && editor.hasPhoto && !editor.busy &&
        !editor.original && !previewViewport.toolActive && editor.retouchMode === "off" &&
        !editor.whiteBalancePicking && !editor.pointColorPicking && !editor.aiObjectPicking
    onComparisonAvailableChanged: { if (!comparisonAvailable) closeBeforeAfter() }
    property bool workspaceHeaderVisible: true
    readonly property bool compactWorkspace: Qt.platform.os === "windows" && width < 820
    property bool wideLeftVisible: true
    property bool wideRightVisible: true
    property string compactPanel: ""
    readonly property bool workspaceLeftVisible: compactWorkspace ? compactPanel === "left" : wideLeftVisible
    readonly property bool developRightVisible: compactWorkspace ? compactPanel === "right" : wideRightVisible
    readonly property bool panelOnly: compactWorkspace && width < 680 && (workspaceLeftVisible || developRightVisible)
    readonly property bool compactFooter: compactWorkspace && width < 600
    property int previousVisibility: Window.Windowed

    function setPanelVisible(left, show) {
        if (compactWorkspace)
            compactPanel = show ? (left ? "left" : "right") : ""
        else if (left)
            wideLeftVisible = show
        else
            wideRightVisible = show
    }

    function toggleFullscreen() {
        if (visibility === Window.FullScreen) {
            visibility = previousVisibility
        } else {
            previousVisibility = visibility === Window.Maximized ? Window.Maximized : Window.Windowed
            visibility = Window.FullScreen
        }
    }
    property bool filmstripVisible: true
    property bool workspaceToolsVisible: true
    readonly property bool workspaceKeysReady: active && !updates.restartPending && !batch.working &&
        !(activeFocusItem instanceof TextInput) && !(activeFocusItem instanceof TextEdit)
    readonly property bool workspacePopupOpen: !!Overlay.overlay && Overlay.overlay.visible
    readonly property bool workspaceKeysAvailable: workspaceKeysReady && !workspacePopupOpen
    readonly property bool libraryKeysAvailable: workspaceKeysReady &&
        (!workspacePopupOpen || (libraryMode && libraryPane.reviewDialogVisible))
    readonly property bool developToolReady: workspaceKeysAvailable && !libraryMode &&
        editor.hasPhoto && !editor.busy && !editor.original
    readonly property bool hasFilmstrip: library.count > 0 || (!libraryMode && library.loading && editor.hasPhoto)
    readonly property int stripHeight: Math.min(Theme.filmstripHeight, Math.floor(height * 0.15))

    component ModuleTab: ApertoneButton {
        id: moduleTab
        Layout.fillHeight: true
        font.pixelSize: 15
        font.weight: highlighted ? Font.DemiBold : Font.Medium
        leftPadding: 10; rightPadding: 10
        Accessible.role: Accessible.PageTab
        Accessible.selected: highlighted
        Accessible.name: text
        contentItem: Label {
            text: moduleTab.text
            color: moduleTab.highlighted || moduleTab.hovered ? Theme.text : Theme.muted
            font: moduleTab.font
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            Accessible.ignored: true
        }
        background: Rectangle {
            color: moduleTab.hovered ? Theme.controlHover : "transparent"
            border.width: moduleTab.visualFocus ? 2 : 0; border.color: Theme.accent
            Rectangle {
                anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
                height: 2; visible: moduleTab.highlighted; color: Theme.accent
            }
        }
    }

    component FooterCommand: ApertoneButton {
        required property string label
        required property string compactText
        property string fullText: label
        flat: true
        text: root.compactFooter ? compactText : fullText
        Accessible.name: label
        ToolTip.visible: root.compactFooter && (hovered || visualFocus)
        ToolTip.text: label
    }

    component PanelToggle: ApertoneButton {
        id: panelToggle
        required property bool leftEdge
        checked: leftEdge ? root.workspaceLeftVisible : root.developRightVisible
        checkable: true
        Layout.preferredWidth: 12
        Layout.fillHeight: true
        padding: 0
        Accessible.name: leftEdge ? qsTranslate("Apertone", "Left panel") : qsTranslate("Apertone", "Right panel")
        ToolTip.visible: hovered || visualFocus
        ToolTip.text: Accessible.name + (leftEdge ? "  F7" : "  F8")
        onClicked: root.setPanelVisible(leftEdge, !(leftEdge ? root.workspaceLeftVisible : root.developRightVisible))
        background: Rectangle { color: panelToggle.hovered || panelToggle.visualFocus ? Theme.controlHover : Theme.panel }
        contentItem: Label {
            text: panelToggle.leftEdge === panelToggle.checked ? "◂" : "▸"
            color: panelToggle.visualFocus ? Theme.accent : Theme.muted
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            Accessible.ignored: true
        }
    }

    function showLibraryView(view, selectCurrent = true) {
        if (selectCurrent && !libraryMode && editor.hasPhoto && library.selectedCount === 0) {
            const row = library.reviewPhotoRow(editor.photoID)
            if (row >= 0)
                library.select(row)
        }
        libraryMode = true
        libraryPane.requestView(view)
    }

    function showDevelop() {
        if (!libraryMode)
            return
        if (libraryPane.canOpenInDevelop)
            libraryPane.openSelectedInDevelop()
        else
            libraryMode = false
    }

    function showBeforeAfter() {
        if (beforeAfterActive) {
            closeBeforeAfter()
            return
        }
        if (!comparisonAvailable || editor.saving) return
        beforeAfterPhoto = editor.photoID
        editor.prepareBeforeAfter()
    }
    function closeBeforeAfter() {
        if (!beforeAfterActive) return
        beforeAfterPhoto = ""
        if (editor.comparison.beforeAfter) editor.closeCompare()
    }
    VersionsDialog { id: processingVersions }
    HistoryDialog { id: processingHistory }
    Window {
        id: reviewDisplay
        objectName: "develop.reviewDisplay"
        title: qsTranslate("Apertone", "Apertone review")
        width: Math.min(900, Screen.desktopAvailableWidth || 900)
        height: Math.min(650, Math.max(1, (Screen.desktopAvailableHeight || 682) - 32))
        minimumWidth: 420; minimumHeight: 300
        color: Theme.canvas
        property real centerX: 0.5
        property real centerY: 0.5
        property bool showReference: false
        function openReview(showPin) {
            showReference = showPin
            if (!visible && Qt.application.screens.length > 1) {
                const other = Qt.application.screens.find(item => item !== root.screen)
                if (other) { screen = other; x = other.virtualX + 40; y = other.virtualY + 40 }
            }
            show()
            raise()
        }
        Shortcut { sequence: "Escape"; onActivated: reviewDisplay.close() }
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 10
            RowLayout {
                Layout.fillWidth: true
                ApertoneComboBox {
                    model: [qsTranslate("Apertone", "Current photo"), qsTranslate("Apertone", "Pinned reference")]
                    currentIndex: reviewDisplay.showReference ? 1 : 0
                    onActivated: reviewDisplay.showReference = currentIndex === 1
                }
                ApertoneButton { text: qsTranslate("Apertone", "Fit"); onClicked: { reviewZoom.value = 1; reviewDisplay.centerX = 0.5; reviewDisplay.centerY = 0.5 } }
                ApertoneSlider { id: reviewZoom; Layout.fillWidth: true; from: 1; to: 4; value: 1; Accessible.name: qsTranslate("Apertone", "Zoom") }
                ApertoneButton { text: qsTranslate("Apertone", "Close"); onClicked: reviewDisplay.close() }
            }
            Label { visible: reviewDisplay.showReference && (editor.referencePhoto.busy || !!editor.referencePhoto.error); text: editor.referencePhoto.busy ? qsTranslate("Apertone", "Rendering reference…") : editor.referencePhoto.error; wrapMode: Text.Wrap; Layout.fillWidth: true }
            ComparePane {
                Layout.fillWidth: true; Layout.fillHeight: true
                title: reviewDisplay.showReference ? (editor.referencePhoto.name || qsTranslate("Apertone", "Pin a reference in Develop or Library")) : editor.filename
                source: !reviewDisplay.visible ? "" : reviewDisplay.showReference ? editor.referencePhoto.source : editor.hasPhoto && !editor.busy ? editor.previewUrl : ""
                zoom: reviewZoom.value; centerX: reviewDisplay.centerX; centerY: reviewDisplay.centerY
                onPanned: function(x, y) { reviewDisplay.centerX = x; reviewDisplay.centerY = y }
            }
        }
    }
    function ensureActiveFocusVisible(scroll) {
        const item = root.activeFocusItem
        const view = scroll ? scroll.contentItem : null
        if (!scroll || !scroll.visible || !item || !view || !view.contentItem)
            return
        let ancestor = item
        while (ancestor && ancestor !== view.contentItem)
            ancestor = ancestor.parent
        if (!ancestor)
            return
        const position = item.mapToItem(view, 0, 0)
        let target = view.contentY
        if (position.y < 0)
            target += position.y
        else if (position.y + item.height > view.height)
            target += position.y + item.height - view.height
        const minimum = view.originY
        const maximum = minimum + Math.max(0, view.contentHeight - view.height)
        view.cancelFlick()
        view.contentY = Math.max(minimum, Math.min(maximum, target))
    }
    function revealActiveFocus() {
        let ancestor = root.activeFocusItem
        while (ancestor) {
            if (ancestor instanceof ScrollView)
                ensureActiveFocusVisible(ancestor)
            ancestor = ancestor.parent
        }
    }
    onActiveFocusItemChanged: Qt.callLater(root.revealActiveFocus)
    Connections {
        target: updates
        function onChanged() {
            if (updates.restartPending) restartWait.open();
            else restartWait.close();
        }
    }
    Dialog {
        id: restartWait; objectName: "restart.wait"
        title: qsTranslate("Apertone", "Preparing update")
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: parent
        width: 430
        ColumnLayout {
            width: parent.width
            Label { text: qsTranslate("Apertone", updates.message); wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { visible: editor.error.length > 0; text: qsTranslate("Apertone", editor.error); color: Theme.negative; wrapMode: Text.Wrap; Layout.fillWidth: true }
            ApertoneButton { visible: editor.saving && !editor.busy; text: qsTranslate("Apertone", "Retry save"); onClicked: editor.retrySave() }
            ApertoneButton { visible: editor.exporting; text: qsTranslate("Apertone", "Cancel export"); onClicked: editor.cancelExport() }
            ApertoneButton { objectName: "restart.cancelImport"; visible: library.importing; text: qsTranslate("Apertone", "Cancel import"); onClicked: library.cancelImport() }
        }
    }
    Connections { target: batch; function onChanged() { if (batch.working) batchWait.open(); else batchWait.close() } }
    Dialog {
        id: batchWait; objectName: "batch.wait"; title: qsTranslate("Apertone", "Batch processing"); modal: true
        closePolicy: Popup.NoAutoClose; anchors.centerIn: parent; width: 450; implicitWidth: 450
        ColumnLayout {
            width: parent.width
            Label { text: qsTranslate("Apertone", batch.status); textFormat: Text.PlainText; wrapMode: Text.Wrap; Layout.fillWidth: true }
            ProgressBar { value: batch.progress; indeterminate: batch.waitingForSave; Layout.fillWidth: true }
            Label { text: qsTranslate("Apertone", editor.error); textFormat: Text.PlainText; visible: batch.waitingForSave && text.length > 0; color: Theme.negative; wrapMode: Text.Wrap; Layout.fillWidth: true }
            ApertoneButton { text: qsTranslate("Apertone", "Retry save"); visible: batch.waitingForSave && editor.saving && !editor.busy; onClicked: editor.retrySave() }
            ApertoneButton { objectName: "batch.cancel"; text: qsTranslate("Apertone", "Cancel"); onClicked: batch.cancel() }
        }
    }
    onClosing: function(close) { const d = exporter.requestClose(); const c = batch.requestClose(); const b = editor.requestClose(); const a = d && c && b ? library.requestClose() : false; close.accepted = a && b && c && d }
    Connections { target: exporter; function onCloseReady() { root.close() } }
    Connections { target: batch; function onCloseReady() { root.close() } }
    Connections { target: library; function onCloseReady() { root.close() } }
    Connections {
        target: editor
        function onPhotoLoaded(id) {
            if (root.beforeAfterPhoto !== id) root.closeBeforeAfter()
            root.libraryMode = false
        }
        function onEditsSaved(id) {
            if (root.beforeAfterPhoto === id && editor.comparison.fullResolution)
                editor.refreshBeforeAfter(id)
        }
    }
    Connections { target: editor; function onCloseReady() { root.close() } }
    Shortcut { enabled: !updates.restartPending && !batch.working; sequences: [StandardKey.Open]; onActivated: fileDialog.open() }
    Shortcut { sequences: [StandardKey.Undo]; enabled: !editor.busy && !updates.restartPending && !batch.working && (processingHistory.visible ? editor.canUndo : root.libraryMode ? batch.canUndo : editor.canUndo || batch.canUndo); onActivated: !processingHistory.visible && (root.libraryMode || !editor.canUndo) ? batch.undo() : editor.undo() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: !editor.busy && !updates.restartPending && !batch.working && (processingHistory.visible ? editor.canRedo : root.libraryMode ? batch.canRedo : editor.canRedo || batch.canRedo); onActivated: !processingHistory.visible && (root.libraryMode || !editor.canRedo) ? batch.redo() : editor.redo() }
    Shortcut { enabled: !updates.restartPending; context: Qt.ApplicationShortcut; sequences: [StandardKey.Quit]; onActivated: root.close() }
    Shortcut { enabled: editor.aiBusy || editor.aiObjectPicking || editor.retouchBusy || editor.retouchState.drawing || editor.whiteBalanceBusy || editor.whiteBalancePicking || editor.creativeBusy || editor.pointColorPicking; sequence: "Escape"; onActivated: { editor.cancelAI(); editor.cancelRetouch(); editor.cancelWhiteBalance(); editor.cancelCreative() } }
    Shortcut { enabled: editor.hasPhoto && !editor.busy && !updates.restartPending && !batch.working; sequence: "\\"; onActivated: editor.original = !editor.original }
    Shortcut { enabled: !updates.restartPending && !batch.working; sequences: Qt.platform.os === "windows" ? ["Ctrl+Shift+F", "F11"] : ["Ctrl+Shift+F"]; autoRepeat: false; onActivated: root.toggleFullscreen() }
    Shortcut { sequences: ["G", "Ctrl+Alt+1"]; context: Qt.ApplicationShortcut; enabled: root.libraryKeysAvailable; autoRepeat: false; onActivated: root.showLibraryView("grid") }
    Shortcut { sequence: "E"; context: Qt.ApplicationShortcut; enabled: root.libraryKeysAvailable && libraryPane.canRequestView("loupe"); autoRepeat: false; onActivated: root.showLibraryView("loupe") }
    Shortcut { sequence: "C"; context: Qt.ApplicationShortcut; enabled: root.libraryKeysAvailable && libraryPane.canRequestView("compare"); autoRepeat: false; onActivated: root.showLibraryView("compare") }
    Shortcut { sequence: "N"; context: Qt.ApplicationShortcut; enabled: root.libraryKeysAvailable && libraryPane.canRequestView("survey"); autoRepeat: false; onActivated: root.showLibraryView("survey") }
    Shortcut { sequences: ["D", "Ctrl+Alt+2"]; enabled: root.workspaceKeysAvailable && !editor.busy && !library.selecting; autoRepeat: false; onActivated: root.showDevelop() }
    Shortcut {
        sequence: "Ctrl+Shift+I"; enabled: root.workspaceKeysAvailable && !library.working
        autoRepeat: false
        onActivated: { root.showLibraryView("grid"); libraryPane.requestImport() }
    }
    Shortcut { sequence: "Ctrl+Shift+E"; enabled: root.workspaceKeysAvailable && exportButton.enabled; autoRepeat: false; onActivated: exportSettings.open() }
    Shortcut { sequence: "F5"; enabled: root.workspaceKeysAvailable; autoRepeat: false; onActivated: root.workspaceHeaderVisible = !root.workspaceHeaderVisible }
    Shortcut { sequence: "F6"; enabled: root.workspaceKeysAvailable && root.hasFilmstrip; autoRepeat: false; onActivated: root.filmstripVisible = !root.filmstripVisible }
    Shortcut { sequence: "F7"; enabled: root.workspaceKeysAvailable; autoRepeat: false; onActivated: root.setPanelVisible(true, !root.workspaceLeftVisible) }
    Shortcut { sequence: "F8"; enabled: root.workspaceKeysAvailable; autoRepeat: false; onActivated: root.setPanelVisible(false, !root.developRightVisible) }
    Shortcut { sequence: "T"; enabled: root.workspaceKeysAvailable; autoRepeat: false; onActivated: root.workspaceToolsVisible = !root.workspaceToolsVisible }
    Shortcut { sequence: "Y"; enabled: root.workspaceKeysAvailable && (root.beforeAfterActive || root.comparisonAvailable && !editor.saving); autoRepeat: false; onActivated: root.showBeforeAfter() }
    Shortcut { sequence: "Escape"; enabled: root.workspaceKeysAvailable && root.beforeAfterActive; autoRepeat: false; onActivated: root.closeBeforeAfter() }
    Shortcut { sequence: "R"; enabled: root.developToolReady; autoRepeat: false; onActivated: editor.cropMode = !editor.cropMode }
    Shortcut { sequence: "W"; enabled: root.developToolReady && !editor.whiteBalanceBusy; autoRepeat: false; onActivated: editor.whiteBalancePicking = !editor.whiteBalancePicking }
    Shortcut {
        sequence: "Z"; enabled: root.workspaceKeysAvailable && !root.libraryMode && editor.zoomAvailable
        autoRepeat: false
        onActivated: {
            if (root.beforeAfterActive) comparisonView.setZoom(comparisonView.mode === "fit" ? "manual" : "fit", 1)
            else editor.oneToOne = !editor.oneToOne
        }
    }
    Shortcut {
        sequences: ["Return", "Enter"]
        context: Qt.ApplicationShortcut
        autoRepeat: false
        enabled: root.active && root.activeFocusItem instanceof Button && root.activeFocusItem.visible && root.activeFocusItem.enabled
        onActivated: root.activeFocusItem.click()
    }
    FileDialog {
        id: fileDialog
        objectName: "main.openDialog"
        title: qsTranslate("Apertone", "Open photo")
        nameFilters: [qsTranslate("Apertone", "Photos (*.3fr *.3FR *.arw *.ARW *.cr2 *.CR2 *.cr3 *.CR3 *.crw *.CRW *.dcr *.DCR *.dcs *.DCS *.dng *.DNG *.erf *.ERF *.fff *.FFF *.gpr *.GPR *.iiq *.IIQ *.k25 *.K25 *.kdc *.KDC *.mef *.MEF *.mos *.MOS *.mrw *.MRW *.nef *.NEF *.nrw *.NRW *.orf *.ORF *.pef *.PEF *.ptx *.PTX *.raf *.RAF *.raw *.RAW *.rw2 *.RW2 *.rwl *.RWL *.sr2 *.SR2 *.srf *.SRF *.srw *.SRW *.jpg *.JPG *.jpeg *.JPEG *.png *.PNG *.tif *.TIF *.tiff *.TIFF *.heic *.HEIC *.heif *.HEIF *.webp *.WEBP *.bmp *.BMP *.gif *.GIF *.avif *.AVIF)"), qsTranslate("Apertone", "All files (*)")]
        onAccepted: editor.openFile(selectedFile)
    }
    PreferencesPanel { id: preferences }
    ExportPanel { id: exportSettings }
    DropArea {
        enabled: !updates.restartPending && !batch.working
        anchors.fill: parent
        onDropped: function(drop) {
            if (drop.hasUrls && drop.urls.length === 1) { editor.openFile(drop.urls[0]); drop.acceptProposedAction() }
        }
    }
    ColumnLayout {
        enabled: !updates.restartPending && !batch.working
        anchors.fill: parent
        spacing: 0
        Rectangle {
            objectName: "main.toolbar"
            visible: root.workspaceHeaderVisible
            Layout.fillWidth: true; Layout.preferredHeight: Theme.toolbarHeight
            color: Theme.panel
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 10; spacing: 8
                Item {
                    implicitWidth: 19; implicitHeight: 19
                    Rectangle { anchors.fill: parent; radius: width / 2; color: "transparent"; border.width: 1; border.color: Theme.text }
                    Rectangle { width: 7; height: 13; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; radius: 4; color: Theme.accent }
                }
                Label { visible: !root.compactFooter; text: "Apertone"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
                Item { Layout.fillWidth: true }
                ApertoneButton { objectName: "settingsButton"; flat: true; text: qsTranslate("Apertone", "Settings"); Accessible.name: text; onClicked: preferences.open() }
                UpdateBell { flat: true }
                Rectangle { implicitWidth: 1; implicitHeight: 20; color: Theme.divider }
                ModuleTab { objectName: "main.libraryButton"; text: qsTranslate("Apertone", "Library"); highlighted: root.libraryMode; onClicked: root.showLibraryView("grid", false) }
                ModuleTab {
                    objectName: "main.developButton"
                    text: qsTranslate("Apertone", "Develop")
                    highlighted: !root.libraryMode
                    onClicked: {
                        if (editor.hasPhoto)
                            root.libraryMode = false
                        else
                            root.showDevelop()
                    }
                }
            }
        }
        Rectangle {
            visible: editor.error.length > 0
            Layout.fillWidth: true; Layout.preferredHeight: visible ? errorLayout.implicitHeight + 24 : 0; color: Theme.errorSurface
            RowLayout {
                id: errorLayout; anchors.fill: parent; anchors.margins: 12
                Label { objectName: "main.editorError"; text: qsTranslate("Apertone", editor.error); color: Theme.negative; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 12 }
                ApertoneButton { objectName: "editor.retrySave"; visible: editor.saving && !editor.busy; text: qsTranslate("Apertone", "Retry save"); onClicked: editor.retrySave() }
            }
        }
        Label {
            visible: editor.offlinePreview && !root.libraryMode
            text: qsTranslate("Apertone", "Editing offline preview. Reconnect the original to export.")
            color: Theme.muted
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            wrapMode: Text.Wrap
        }
        RowLayout {
            visible: editor.exporting
            Layout.fillWidth: true; Layout.margins: visible ? 10 : 0
            Label { text: qsTranslate("Apertone", "Export"); color: Theme.accent }
            ProgressBar { value: editor.exportProgress; Layout.fillWidth: true }
            ApertoneButton { text: qsTranslate("Apertone", "Cancel"); onClicked: editor.cancelExport() }
        }
        ColumnLayout {
            visible: exporter.working || exporter.status.length > 0 || exporter.error.length > 0
            Layout.fillWidth: true; Layout.margins: 10
            RowLayout {
                Label { text: qsTranslate("Apertone", exporter.status); textFormat: Text.PlainText; elide: Text.ElideRight; Layout.fillWidth: true }
                Label { text: exporter.completed + " / " + exporter.total }
                ApertoneButton { objectName: "export.pause"; visible: exporter.working && !exporter.paused; text: exporter.pausePending ? qsTranslate("Apertone", "Pause after this file…") : qsTranslate("Apertone", "Pause"); enabled: !exporter.pausePending; onClicked: exporter.pause() }
                ApertoneButton { objectName: "export.resume"; visible: exporter.paused; text: qsTranslate("Apertone", "Resume"); onClicked: exporter.resume() }
                ApertoneButton { objectName: "export.cancel"; visible: exporter.working; text: qsTranslate("Apertone", "Cancel"); onClicked: exporter.cancel() }
                ApertoneButton { text: qsTranslate("Apertone", "Settings / errors"); onClicked: exportSettings.open() }
            }
            Label { objectName: "export.currentFilename"; visible: exporter.working; text: exporter.currentFile; textFormat: Text.PlainText; elide: Text.ElideMiddle; Layout.fillWidth: true }
            ProgressBar { objectName: "export.progress"; visible: exporter.working; value: exporter.progress; indeterminate: exporter.waitingForSave; Layout.fillWidth: true }
            Label { visible: exporter.error.length > 0; text: qsTranslate("Apertone", exporter.error); textFormat: Text.PlainText; color: Theme.negative; wrapMode: Text.Wrap; Layout.fillWidth: true }
            ScrollView {
                objectName: "export.failuresScroll"
                visible: exporter.failures.length > 0
                Layout.fillWidth: true; Layout.preferredHeight: 65
                TextArea { objectName: "export.failures"; readOnly: true; text: exporter.failures.join("\n"); textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap; color: Theme.negative }
            }
            ApertoneButton { objectName: "export.retrySave"; visible: exporter.waitingForSave && editor.saving && !editor.busy; text: qsTranslate("Apertone", "Retry save"); onClicked: editor.retrySave() }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            spacing: 0
            PanelToggle { objectName: "main.toggleLeftPanel"; leftEdge: true }
            Item { id: moduleWorkspace; Layout.fillWidth: true; Layout.fillHeight: true }
            PanelToggle { objectName: "main.toggleRightPanel"; leftEdge: false }
        }
        LibraryPane {
            id: libraryPane
            objectName: "library.pane"
            parent: moduleWorkspace
            anchors.fill: parent
            visible: root.libraryMode
            leftPanelVisible: root.workspaceLeftVisible
            rightPanelVisible: root.developRightVisible
            compactImportSlot: footerImportSlot.visible ? footerImportSlot : null
            panelOnly: root.panelOnly
            viewToolbarVisible: root.workspaceToolsVisible
        }
        SplitView {
            visible: !root.libraryMode
            id: panels
            objectName: "develop.panels"
            parent: moduleWorkspace
            anchors.fill: parent
            orientation: Qt.Horizontal
            handle: Rectangle { implicitWidth: 1; color: Theme.divider }
            Rectangle {
                objectName: "develop.presetsPane"
                visible: root.workspaceLeftVisible
                SplitView.preferredWidth: Theme.sidebarWidth; SplitView.minimumWidth: 170; SplitView.maximumWidth: root.panelOnly ? panels.width : 320; SplitView.fillWidth: root.panelOnly; color: Theme.panel
                ColumnLayout {
                    id: navigator
                    objectName: "develop.navigator"
                    anchors { top: parent.top; left: parent.left; right: parent.right; margins: Theme.panelPadding }
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTranslate("Apertone", "Navigator")
                            font.weight: Font.DemiBold; color: Theme.text
                            Layout.fillWidth: true; elide: Text.ElideRight
                        }
                        ApertoneTextField {
                            id: zoomPercent
                            objectName: "develop.zoomPercent"
                            Layout.preferredWidth: 60; Layout.minimumWidth: 60; Layout.preferredHeight: Theme.compactControlHeight
                            leftPadding: 4; rightPadding: 4
                            enabled: editor.zoomAvailable
                            horizontalAlignment: Text.AlignRight
                            Accessible.name: qsTranslate("Apertone", "Zoom") + " %"
                            property bool modified: false
                            validator: DoubleValidator { bottom: editor.minimumZoomFactor * 100; top: editor.maximumZoomFactor * 100; decimals: 2; notation: DoubleValidator.StandardNotation; locale: "C" }
                            Binding {
                                target: zoomPercent; property: "text"
                                value: Number(((root.beforeAfterActive ? comparisonView.effectiveZoom : previewViewport.effectiveZoom) * 100).toFixed(2)).toString()
                                when: !zoomPercent.activeFocus
                            }
                            function commitZoom() {
                                if (modified && acceptableInput) {
                                    if (root.beforeAfterActive) comparisonView.setZoom("manual", Number(text) / 100)
                                    else previewViewport.requestScale(Number(text) / 100)
                                }
                                modified = false
                            }
                            onTextEdited: modified = true
                            onAccepted: { commitZoom(); previewViewport.forceActiveFocus() }
                            onEditingFinished: commitZoom()
                            Keys.onEscapePressed: { modified = false; previewViewport.forceActiveFocus() }
                        }
                        Label { text: "%"; color: Theme.muted }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(112, width * 0.62, panels.height * 0.18)
                        color: Theme.canvas
                        Image {
                            id: navImage
                            objectName: "develop.navigatorPreview"
                            anchors.fill: parent
                            source: navigator.visible && editor.hasPhoto && !editor.busy ? editor.previewUrl : ""
                            sourceSize: Qt.size(Math.max(1, Math.round(width * Screen.devicePixelRatio)),
                                                Math.max(1, Math.round(height * Screen.devicePixelRatio)))
                            asynchronous: true; cache: false; fillMode: Image.PreserveAspectFit
                        }
                        Item {
                            width: navImage.paintedWidth; height: navImage.paintedHeight
                            anchors.centerIn: navImage
                            visible: navImage.status === Image.Ready
                            readonly property point topLeft: root.beforeAfterActive
                                ? Qt.point(comparisonView.visibleRect.x, comparisonView.visibleRect.y) : previewViewport.sourceAt(Qt.point(0, 0))
                            readonly property point bottomRight: root.beforeAfterActive
                                ? Qt.point(comparisonView.visibleRect.x + comparisonView.visibleRect.width, comparisonView.visibleRect.y + comparisonView.visibleRect.height)
                                : previewViewport.sourceAt(Qt.point(previewViewport.width, previewViewport.height))
                            Rectangle {
                                objectName: "develop.navigatorBounds"
                                visible: root.beforeAfterActive ? comparisonView.mode !== "fit" : previewViewport.pixelMode
                                x: parent.topLeft.x * parent.width; y: parent.topLeft.y * parent.height
                                width: (parent.bottomRight.x - parent.topLeft.x) * parent.width
                                height: (parent.bottomRight.y - parent.topLeft.y) * parent.height
                                color: "transparent"; border.width: 1; border.color: "white"
                            }
                            MouseArea {
                                id: navigatorMap
                                objectName: "develop.navigatorMap"
                                anchors.fill: parent
                                enabled: editor.zoomAvailable && !previewViewport.toolActive && editor.retouchMode === "off"
                                activeFocusOnTab: true
                                Accessible.role: Accessible.Pane
                                Accessible.name: qsTranslate("Apertone", "Navigator")
                                cursorShape: Qt.CrossCursor
                                function moveViewport(dx, dy) {
                                    if (root.beforeAfterActive) {
                                        if (comparisonView.mode === "fit") comparisonView.setZoom("manual", 1)
                                        comparisonView.panTo(comparisonView.centerX + dx * comparisonView.visibleRect.width / 4,
                                                            comparisonView.centerY + dy * comparisonView.visibleRect.height / 4)
                                        return
                                    }
                                    if (!previewViewport.pixelMode) previewViewport.requestScale(1)
                                    previewViewport.panBy(dx * previewViewport.width / 4, dy * previewViewport.height / 4)
                                }
                                Keys.onLeftPressed: moveViewport(-1, 0)
                                Keys.onRightPressed: moveViewport(1, 0)
                                Keys.onUpPressed: moveViewport(0, -1)
                                Keys.onDownPressed: moveViewport(0, 1)
                                function navigate(mouse) {
                                    if (width <= 0 || height <= 0) return
                                    if (root.beforeAfterActive) {
                                        if (comparisonView.mode === "fit") comparisonView.setZoom("manual", 1)
                                        comparisonView.panTo(mouse.x / width, mouse.y / height)
                                        return
                                    }
                                    if (!previewViewport.pixelMode) previewViewport.requestScale(1)
                                    previewViewport.pendingSource = Qt.point(Math.max(0, Math.min(1, mouse.x / width)),
                                                                            Math.max(0, Math.min(1, mouse.y / height)))
                                    previewViewport.pendingPoint = Qt.point(previewViewport.width / 2, previewViewport.height / 2)
                                    previewViewport.pendingZoom = previewViewport.effectiveZoom
                                    previewViewport.pendingPlacement = true
                                    previewViewport.schedulePlacement()
                                }
                                onPressed: function(mouse) { navigate(mouse) }
                                onPositionChanged: function(mouse) { if (pressed) navigate(mouse) }
                                Rectangle {
                                    anchors.fill: parent
                                    visible: navigatorMap.activeFocus
                                    color: "transparent"; border.width: 2; border.color: Theme.accent
                                }
                            }
                        }
                    }
                    GridLayout {
                        objectName: "develop.zoomToolbar"
                        Layout.fillWidth: true; columnSpacing: 3; rowSpacing: 2
                        columns: width >= zoomFit.implicitWidth + zoomFill.implicitWidth + zoomActual.implicitWidth + 2 * columnSpacing ? 3 : 2
                        ApertoneButton {
                            id: zoomFit
                            objectName: "develop.fit"
                            text: qsTranslate("Apertone", "Fit")
                            Layout.fillWidth: true; leftPadding: 3; rightPadding: 3
                            checked: root.beforeAfterActive ? comparisonView.mode === "fit" : editor.zoomMode === "fit"
                            enabled: editor.hasPhoto && !editor.busy
                            onClicked: { if (root.beforeAfterActive) comparisonView.setZoom("fit"); else editor.zoomMode = "fit" }
                        }
                        ApertoneButton {
                            id: zoomFill
                            objectName: "develop.fill"
                            text: qsTranslate("Apertone", "FILL")
                            Layout.fillWidth: true; leftPadding: 3; rightPadding: 3
                            checked: root.beforeAfterActive ? comparisonView.mode === "fill" : editor.zoomMode === "fill"
                            enabled: editor.zoomAvailable
                            onClicked: { if (root.beforeAfterActive) comparisonView.setZoom("fill"); else editor.zoomMode = "fill" }
                        }
                        ApertoneButton {
                            id: zoomActual
                            objectName: "develop.actualPixels"
                            text: "100%"
                            Accessible.name: qsTranslate("Apertone", "Zoom") + " 100%"
                            Layout.fillWidth: true; Layout.columnSpan: parent.columns === 2 ? 2 : 1; leftPadding: 3; rightPadding: 3
                            checked: root.beforeAfterActive ? comparisonView.mode === "manual" && comparisonView.manualZoom === 1 : editor.oneToOne
                            enabled: editor.zoomAvailable
                            onClicked: { if (root.beforeAfterActive) comparisonView.setZoom("manual", 1); else editor.oneToOne = true }
                        }
                    }
                    ApertoneTextField {
                        id: sourceName
                        objectName: "develop.sourceFilename"
                        visible: editor.hasPhoto
                        text: editor.filename; readOnly: true; selectByMouse: true; activeFocusOnTab: true
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        Layout.preferredHeight: Theme.compactControlHeight
                        color: Theme.muted; font.pixelSize: 10
                        Accessible.name: text
                        onActiveFocusChanged: if (!activeFocus) cursorPosition = 0
                        ToolTip {
                            visible: sourceName.contentWidth > sourceName.width - sourceName.leftPadding - sourceName.rightPadding &&
                                     (sourceName.hovered || sourceName.activeFocus)
                            width: Math.min(root.width - 24, Math.max(180, sourceName.width * 1.5))
                            text: sourceName.text
                            contentItem: Text {
                                text: sourceName.text; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere
                                color: Theme.text; font.pixelSize: 11
                            }
                            background: Rectangle { color: Theme.panelRaised; border.color: Theme.divider; radius: Theme.radius }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                }
                ScrollView {
                    id: presetsScroll
                    objectName: "develop.presetsScroll"
                    Component.onCompleted: ScrollBar.vertical.objectName = "develop.presetsBar"
                    anchors { top: navigator.bottom; bottom: parent.bottom; left: parent.left; right: parent.right; margins: Theme.panelPadding }
                    clip: true
                    rightPadding: effectiveScrollBarWidth
                    contentWidth: availableWidth
                    ColumnLayout {
                        width: presetsScroll.availableWidth
                        height: Math.max(implicitHeight, presetsScroll.availableHeight)
                        spacing: 9
                        PresetPanel { id: presetSettings; libraryModel: library; Layout.fillWidth: true; Layout.fillHeight: true }
                        PanelAction {
                            objectName: "develop.versions"
                            text: qsTranslate("Apertone", "Processing versions")
                            enabled: editor.hasPhoto && !editor.busy && !batch.working && !exporter.working && !updates.restartPending
                            Layout.fillWidth: true
                            onClicked: processingVersions.open()
                        }
                        ApertoneButton {
                            objectName: "develop.history"
                            text: qsTranslate("Apertone", "History")
                            enabled: editor.hasPhoto && !editor.busy && !batch.working && !exporter.working && !updates.restartPending
                            Layout.fillWidth: true
                            onClicked: processingHistory.open()
                        }
                        Label { text: qsTranslate("Apertone", "SOURCE PHOTO"); color: Theme.faint; font.pixelSize: 9; font.letterSpacing: 1; Layout.topMargin: 10 }
                        Label { text: editor.dimensions; color: Theme.faint; font.pixelSize: 11 }
                        MetadataRow { objectName: "develop.decode"; visible: editor.hasPhoto; label: qsTranslate("Apertone", "Decode"); value: editor.decoderSummary; Layout.fillWidth: true }
                        Label { text: qsTranslate("Apertone", "Original unchanged"); color: Theme.muted; font.pixelSize: 10; visible: editor.hasPhoto }
                        Label { text: qsTranslate("Apertone", "CATALOG"); color: Theme.faint; font.pixelSize: 9; font.letterSpacing: 1 }
                        Label { text: editor.catalogPath; color: Theme.faint; font.pixelSize: 10; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                    }
                }
            }
            Rectangle {
                objectName: "develop.canvasPane"
                visible: !root.panelOnly
                SplitView.fillWidth: true; SplitView.minimumWidth: 360; color: Theme.canvas
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: Theme.panelSpacing
                    RowLayout {
                        visible: !root.beforeAfterActive
                        Layout.fillWidth: true; Layout.fillHeight: true
                        ComparePane {
                            id: referencePane
                            onCanvas: true
                            objectName: "develop.referencePane"
                            visible: !!editor.referencePhoto.source
                            Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 1
                            title: qsTranslate("Apertone", "Reference") + " · " + editor.referencePhoto.name
                            source: visible ? editor.referencePhoto.source : ""
                            zoom: 1; centerX: 0.5; centerY: 0.5
                        }
                    Item {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: 84; Layout.minimumHeight: 84
                        PhotoViewport {
                            id: previewViewport
                            objectName: "develop.viewport"
                            controller: editor
                            anchors.fill: parent
                        Image {
                            id: previewImage
                            objectName: "develop.preview"
                            width: previewViewport.imageItemWidth
                            height: previewViewport.imageItemHeight
                            x: (previewViewport.contentWidth - width) / 2
                            y: (previewViewport.contentHeight - height) / 2
                            source: editor.previewUrl; fillMode: Image.PreserveAspectFit; cache: false; visible: editor.hasPhoto && !editor.busy
                            horizontalAlignment: Image.AlignHCenter
                            verticalAlignment: Image.AlignVCenter
                            MouseArea {
                                anchors.fill: parent
                                enabled: editor.aiObjectPicking && !editor.busy
                                z: 1
                                cursorShape: Qt.CrossCursor
                                onClicked: function(mouse) {
                                    const left = (width - previewImage.paintedWidth) / 2
                                    const top = (height - previewImage.paintedHeight) / 2
                                    if (previewImage.paintedWidth > 0 && previewImage.paintedHeight > 0 &&
                                        mouse.x >= left && mouse.x <= left + previewImage.paintedWidth &&
                                        mouse.y >= top && mouse.y <= top + previewImage.paintedHeight)
                                        editor.pickAIMask((mouse.x - left) / previewImage.paintedWidth,
                                                          (mouse.y - top) / previewImage.paintedHeight)
                                }
                            }
                            MouseArea {
                                objectName: "develop.whiteBalance.sampleArea"
                                anchors.fill: parent
                                enabled: editor.whiteBalancePicking && editor.hasPhoto && !editor.busy
                                z: 2
                                cursorShape: editor.whiteBalanceCanvasReady ? Qt.CrossCursor : Qt.WaitCursor
                                onClicked: function(mouse) {
                                    if (!editor.whiteBalanceCanvasReady || previewImage.status !== Image.Ready) return
                                    const left = (width - previewImage.paintedWidth) / 2
                                    const top = (height - previewImage.paintedHeight) / 2
                                    if (previewImage.paintedWidth > 0 && previewImage.paintedHeight > 0 &&
                                        mouse.x >= left && mouse.x <= left + previewImage.paintedWidth &&
                                        mouse.y >= top && mouse.y <= top + previewImage.paintedHeight)
                                        editor.sampleWhiteBalance((mouse.x - left) / previewImage.paintedWidth,
                                                                  (mouse.y - top) / previewImage.paintedHeight)
                                }
                            }
                            MouseArea {
                                objectName: "develop.pointColor.sampleArea"
                                anchors.fill: parent; z: 3
                                enabled: editor.pointColorPicking && editor.hasPhoto && !editor.busy
                                cursorShape: editor.pointColorCanvasReady ? Qt.CrossCursor : Qt.WaitCursor
                                onClicked: function(mouse) {
                                    if (!editor.pointColorCanvasReady || previewImage.status !== Image.Ready) return
                                    const left=(width-previewImage.paintedWidth)/2, top=(height-previewImage.paintedHeight)/2
                                    if(previewImage.paintedWidth>0 && previewImage.paintedHeight>0 && mouse.x>=left && mouse.y>=top &&
                                       mouse.x<=left+previewImage.paintedWidth && mouse.y<=top+previewImage.paintedHeight)
                                        editor.samplePointColor((mouse.x-left)/previewImage.paintedWidth,(mouse.y-top)/previewImage.paintedHeight)
                                }
                            }
                            MouseArea {
                                objectName: "develop.targetedArea"
                                anchors.fill: parent
                                enabled: !editor.cropMode && !editor.guidedMode && editor.targetedTool !== "none" && editor.hasPhoto && !editor.busy && !editor.original
                                cursorShape: Qt.SizeVerCursor
                                property real startY: 0
                                property bool adjusting: false
                                onPressed: function(mouse) {
                                    const left=(width-previewImage.paintedWidth)/2, top=(height-previewImage.paintedHeight)/2
                                    if(previewImage.paintedWidth<=0||previewImage.paintedHeight<=0||mouse.x<left||mouse.x>left+previewImage.paintedWidth||mouse.y<top||mouse.y>top+previewImage.paintedHeight){mouse.accepted=false;return}
                                    startY=mouse.y;adjusting=editor.beginTargeted((mouse.x-left)/previewImage.paintedWidth,(mouse.y-top)/previewImage.paintedHeight)
                                }
                                onPositionChanged: function(mouse) { if(pressed&&adjusting&&previewImage.paintedHeight>0)editor.adjustTargeted((startY-mouse.y)/previewImage.paintedHeight) }
                                onReleased: { adjusting=false;editor.endTargeted() }
                                onCanceled: { adjusting=false;editor.endTargeted() }
                            }
                        }
                        RetouchOverlay {
                            x: (previewImage.width-previewImage.paintedWidth)/2
                            y: (previewImage.height-previewImage.paintedHeight)/2
                            // These tools currently operate in Fit. Do not allocate
                            // hidden Canvas backing images at magnified dimensions.
                            width: previewViewport.pixelMode ? 0 : previewImage.paintedWidth
                            height: previewViewport.pixelMode ? 0 : previewImage.paintedHeight
                        }
                        LocalOverlay {
                            photoWidth: editor.localMode || !previewViewport.pixelMode ? previewImage.paintedWidth : 0
                            photoHeight: editor.localMode || !previewViewport.pixelMode ? previewImage.paintedHeight : 0
                            width: Math.max(84, photoWidth); height: Math.max(84, photoHeight)
                            x: previewImage.x + (previewImage.width-width)/2
                            y: previewImage.y + (previewImage.height-height)/2
                            paintBounds: previewViewport.visibleBounds(x,y,width,height)
                        }
                        TransformOverlay {
                            x: previewImage.x + (previewImage.width-previewImage.paintedWidth)/2
                            y: previewImage.y + (previewImage.height-previewImage.paintedHeight)/2
                            width: editor.guidedMode || !previewViewport.pixelMode ? previewImage.paintedWidth : 0
                            height: editor.guidedMode || !previewViewport.pixelMode ? previewImage.paintedHeight : 0
                            paintBounds: previewViewport.visibleBounds(x,y,width,height)
                        }
                        CropOverlay {
                            x: previewImage.x + (previewImage.width-previewImage.paintedWidth)/2
                            y: previewImage.y + (previewImage.height-previewImage.paintedHeight)/2
                            width: editor.cropMode || !previewViewport.pixelMode ? previewImage.paintedWidth : 0
                            height: editor.cropMode || !previewViewport.pixelMode ? previewImage.paintedHeight : 0
                            paintBounds: previewViewport.visibleBounds(x,y,width,height)
                        }
                        }
                        ColumnLayout {
                            anchors.centerIn: parent; spacing: 12; visible: !editor.hasPhoto && !editor.busy
                            Item {
                                Layout.preferredWidth: 44; Layout.preferredHeight: 44; Layout.alignment: Qt.AlignHCenter
                                Rectangle { anchors.fill: parent; radius: width / 2; color: "transparent"; border.width: 2; border.color: Theme.canvasText }
                                Rectangle { width: 16; height: 30; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; radius: 8; color: Theme.accent }
                            }
                            Label { text: qsTranslate("Apertone", "It starts with a photo."); color: Theme.canvasText; font.pixelSize: 22; font.weight: Font.Light; Layout.alignment: Qt.AlignHCenter }
                            Label { text: qsTranslate("Apertone", "Open a photo. Find your light."); color: Theme.canvasMuted; font.pixelSize: 11; Layout.alignment: Qt.AlignHCenter }
                            ApertoneButton { text: qsTranslate("Apertone", "Open photo"); Layout.alignment: Qt.AlignHCenter; onClicked: fileDialog.open() }
                            Label { text: qsTranslate("Apertone", "RAW and common images"); color: Theme.canvasMuted; font.pixelSize: 10; Layout.alignment: Qt.AlignHCenter }
                        }
                        ColumnLayout {
                            anchors.centerIn: parent; visible: editor.busy; spacing: 16; width: Math.min(parent.width - 60, 300)
                            BusyIndicator { running: editor.busy; Layout.alignment: Qt.AlignHCenter }
                            Label { text: qsTranslate("Apertone", "Decoding image pixels"); color: Theme.canvasText; font.pixelSize: 12; Layout.alignment: Qt.AlignHCenter }
                            ProgressBar { value: editor.progress; Layout.fillWidth: true }
                            ApertoneButton { objectName: "editor.cancelLoad"; text: qsTranslate("Apertone", "Cancel"); onClicked: editor.cancelLoad(); Layout.alignment: Qt.AlignHCenter }
                        }
                    }
                    }
                    DevelopComparison {
                        id: comparisonView
                        objectName: "develop.comparison"
                        visible: root.beforeAfterActive
                        Layout.fillWidth: true; Layout.fillHeight: true
                    }
                    Rectangle { visible: root.workspaceToolsVisible; Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                    Label { visible: editor.detailDiagnostic.length || editor.original || editor.presetPreview; Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight; text: editor.detailDiagnostic.length ? qsTranslate("Apertone", editor.detailDiagnostic) : editor.original ? qsTranslate("Apertone", "ORIGINAL PHOTO") : editor.presetPreview ? qsTranslate("Apertone", "PRESET PREVIEW") : qsTranslate("Apertone", "PREVIEW"); color: Theme.canvasMuted; font.pixelSize: 9; font.letterSpacing: 2 }
                    Flow {
                        objectName: "develop.viewToolbar"
                        visible: root.workspaceToolsVisible
                        Layout.fillWidth: true; Layout.minimumWidth: 0; spacing: Theme.panelSpacing
                        ApertoneButton { objectName: "develop.processedButton"; text: qsTranslate("Apertone", "Develop"); checked: !editor.original && !root.beforeAfterActive; enabled: editor.hasPhoto && !editor.busy; onClicked: { root.closeBeforeAfter(); editor.original = false } }
                        ApertoneButton { objectName: "develop.originalButton"; text: qsTranslate("Apertone", "Original"); checkable: true; checked: editor.original; enabled: editor.hasPhoto && !editor.busy; onClicked: editor.original = true }
                        ApertoneButton {
                            objectName: "develop.beforeAfterButton"
                            text: qsTranslate("Apertone", "Before / After")
                            checked: root.beforeAfterActive
                            enabled: root.beforeAfterActive || root.comparisonAvailable && !editor.saving
                            onClicked: root.showBeforeAfter()
                        }
                        ApertoneButton {
                            objectName: "develop.splitComparison"
                            visible: root.beforeAfterActive
                            text: qsTranslate("Apertone", "Split comparison")
                            checked: comparisonView.split
                            onClicked: comparisonView.split = !comparisonView.split
                        }
                        ApertoneButton {
                            objectName: "develop.referenceButton"
                            text: editor.referencePhoto.source ? qsTranslate("Apertone", "Clear reference") : qsTranslate("Apertone", "Pin reference")
                            enabled: !!editor.referencePhoto.source || editor.hasPhoto && !editor.busy && !editor.referencePhoto.busy
                            onClicked: { root.closeBeforeAfter(); editor.referencePhoto.source ? editor.clearReference() : editor.pinReference(editor.photoID) }
                        }
                        ApertoneButton { objectName: "develop.secondDisplay"; text: qsTranslate("Apertone", "Review window"); enabled: editor.hasPhoto; onClicked: reviewDisplay.openReview(false) }
                    }
                    Label { visible: editor.referencePhoto.busy || !!editor.referencePhoto.error; text: editor.referencePhoto.busy ? qsTranslate("Apertone", "Rendering reference…") : editor.referencePhoto.error; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    Label {
                        visible: editor.whiteBalancePicking
                        text: qsTranslate("Apertone", "Click a neutral gray area. Escape cancels the eyedropper.")
                        color: Theme.accent; font.pixelSize: 11
                        wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }
                    Label { visible:editor.targetedTool!=="none";text:qsTranslate("Apertone", "Click a tone or color and drag up / down");color:Theme.accent;font.pixelSize:11;Layout.alignment:Qt.AlignHCenter }
                }
            }
            Rectangle {
                objectName: "develop.adjustmentsPane"
                visible: root.developRightVisible
                SplitView.preferredWidth: Theme.inspectorWidth; SplitView.minimumWidth: 250; SplitView.maximumWidth: root.panelOnly ? panels.width : 380; SplitView.fillWidth: root.panelOnly; color: Theme.panel
                ColumnLayout {
                    id: inspectorHeader
                    anchors { top: parent.top; left: parent.left; right: parent.right; margins: Theme.panelPadding }
                    spacing: 4
                    ScrollView {
                        id: histogramScroll
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(histogramPanel.implicitHeight, panels.height * 0.44)
                        clip: true
                        rightPadding: effectiveScrollBarWidth
                        contentWidth: availableWidth
                        Connections {
                            target: root
                            function onActiveFocusItemChanged() { Qt.callLater(root.ensureActiveFocusVisible, histogramScroll) }
                        }
                        HistogramPanel { id: histogramPanel; compact: true; width: histogramScroll.availableWidth }
                    }
                    RowLayout {
                        objectName: "develop.toolStrip"
                        Layout.fillWidth: true; spacing: 3
                        enabled: editor.hasPhoto && !editor.busy && !editor.editingLocked && !editor.original
                        ApertoneButton {
                            objectName: "develop.tool.crop"; text: qsTranslate("Apertone", "Crop")
                            leftPadding: 3; rightPadding: 3
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            highlighted: editor.cropMode
                            onClicked: {
                                adjustmentsScroll.revealSection(geometrySection)
                                if (editor.correctionStates.crop) editor.cropMode = !editor.cropMode
                            }
                        }
                        ApertoneButton {
                            objectName: "develop.tool.retouch"; text: qsTranslate("Apertone", "Retouch")
                            leftPadding: 3; rightPadding: 3
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            highlighted: editor.retouchMode !== "off"
                            onClicked: {
                                adjustmentsScroll.revealSection(retouchSection)
                                editor.retouchMode = editor.retouchMode === "off" ? "heal" : "off"
                            }
                        }
                        ApertoneButton {
                            objectName: "develop.tool.masking"; text: qsTranslate("Apertone", "Masking")
                            leftPadding: 3; rightPadding: 3
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            highlighted: editor.localMode
                            onClicked: {
                                editor.localMode = !editor.localMode
                                maskPanel.controlsOpen = editor.localMode
                                if (editor.localMode) adjustmentsScroll.scrollToControl(maskPanel)
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                }
                ApertoneButton {
                    id: resetDevelop
                    objectName: "develop.resetButton"; text: qsTranslate("Apertone", "Reset all edits")
                    anchors { bottom: parent.bottom; left: parent.left; right: parent.right; margins: Theme.panelPadding }
                    enabled: editor.hasPhoto && !editor.busy; onClicked: editor.resetEdits()
                }
                ScrollView {
                    id: adjustmentsScroll
                    objectName: "develop.adjustmentsScroll"
                    readonly property var sections: [basicSection, curveSection, colorSection,
                        pointColorSection, gradingSection, blackWhiteSection, detailSection,
                        opticsSection, geometrySection, effectsSection, calibrationSection, retouchSection]
                    function revealSection(section) {
                        section.expanded = true
                        section.focusHeader()
                        scrollToControl(section)
                    }
                    function scrollToControl(control) {
                        Qt.callLater(function() {
                            const view = adjustmentsScroll.contentItem
                            const target = control.mapToItem(view.contentItem, 0, 0).y
                            const maximum = view.originY + Math.max(0, view.contentHeight - view.height)
                            view.cancelFlick()
                            view.contentY = Math.max(view.originY, Math.min(maximum, target))
                        })
                    }
                    Component.onCompleted: ScrollBar.vertical.objectName = "develop.adjustmentsBar"
                    anchors { top: inspectorHeader.bottom; bottom: resetDevelop.top; left: parent.left; right: parent.right; margins: Theme.panelPadding }
                    clip: true
                    rightPadding: effectiveScrollBarWidth
                    contentWidth: availableWidth
                    ColumnLayout {
                        width: adjustmentsScroll.availableWidth
                        spacing: Theme.panelSpacing
                        DevelopSection {
                            id: basicSection
                            objectName: "develop.section.basic"
                            title: qsTranslate("Apertone", "Basic")
                            FileDialog {
                                id: cameraProfileFile
                                title: qsTranslate("Apertone", "Choose a DNG camera profile")
                                nameFilters: ["DNG camera profiles (*.dcp *.DCP)"]
                                onAccepted: editor.setCameraProfile(selectedFile)
                            }
                            Label {
                                visible: editor.cameraProfileAvailable && editor.cameraProfileName.length > 0
                                text: editor.cameraProfileName
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            RowLayout {
                                visible: editor.cameraProfileAvailable
                                Layout.fillWidth: true
                                ApertoneButton {
                                    objectName: "develop.cameraProfile"
                                    text: qsTranslate("Apertone", "Load camera profile")
                                    enabled: !editor.busy && !editor.editingLocked
                                    Layout.fillWidth: true
                                    onClicked: cameraProfileFile.open()
                                }
                                ApertoneButton {
                                    objectName: "develop.clearCameraProfile"
                                    visible: editor.cameraProfileName.length > 0
                                    text: qsTranslate("Apertone", "Clear")
                                    enabled: !editor.busy && !editor.editingLocked
                                    onClicked: editor.setCameraProfile("")
                                }
                            }
                            ApertoneButton {
                                objectName: "develop.autoAdjust"
                                text: qsTranslate("Apertone", "Auto Adjust"); Layout.fillWidth: true
                                enabled: editor.hasPhoto && !editor.busy && !editor.original && !editor.creativeBusy
                                onClicked: editor.autoAdjust()
                            }
                            RowLayout {
                                ApertoneCheckBox { objectName: "develop.whiteBalance.enabled"; Accessible.name: qsTranslate("Apertone", "Enable white balance"); checked: editor.correctionStates.whiteBalance; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setCorrectionEnabled("whiteBalance",checked); padding: 0 }
                                Label { text: qsTranslate("Apertone", "WHITE BALANCE"); color: Theme.muted; font.pixelSize: 9; font.letterSpacing: 1 }
                            }
                            ApertoneComboBox {
                                objectName: "develop.whiteBalance.preset"
                                Layout.fillWidth: true
                                textRole: "label"
                                model: [{ value: "As Shot", label: qsTranslate("Apertone", "As Shot") },{ value: "Daylight", label: qsTranslate("Apertone", "Daylight") },{ value: "Cloudy", label: qsTranslate("Apertone", "Cloudy") },{ value: "Shade", label: qsTranslate("Apertone", "Shade") },{ value: "Tungsten", label: qsTranslate("Apertone", "Tungsten") },{ value: "Fluorescent", label: qsTranslate("Apertone", "Fluorescent") },{ value: "Custom", label: qsTranslate("Apertone", "Custom") }]
                                currentIndex: editor.customWhiteBalance ? 6 : 0
                                enabled: editor.hasPhoto && !editor.busy && editor.correctionStates.whiteBalance
                                onActivated: editor.setWhiteBalancePreset(model[currentIndex].value)
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                ApertoneButton {
                                    objectName: "develop.whiteBalance.auto"
                                    text: qsTranslate("Apertone", "Auto")
                                    Accessible.name: qsTranslate("Apertone", "Auto white balance")
                                    Layout.fillWidth: true
                                    enabled: editor.hasPhoto && !editor.busy && !editor.original &&
                                             !editor.exporting && !editor.whiteBalanceBusy && editor.correctionStates.whiteBalance
                                    onClicked: editor.autoWhiteBalance()
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTranslate("Apertone", "Estimates white balance from the visible crop. Strongly colored scenes may need manual adjustment.")
                                }
                                ApertoneButton {
                                    objectName: "develop.whiteBalance.eyedropper"
                                    text: qsTranslate("Apertone", "Eyedropper")
                                    Accessible.name: qsTranslate("Apertone", "White balance eyedropper")
                                    Layout.fillWidth: true
                                    checkable: true; checked: editor.whiteBalancePicking
                                    enabled: editor.hasPhoto && !editor.busy && !editor.original &&
                                             !editor.exporting && !editor.whiteBalanceBusy && editor.correctionStates.whiteBalance
                                    onClicked: editor.whiteBalancePicking = !editor.whiteBalancePicking
                                }
                            }
                            RowLayout {
                                visible: editor.whiteBalanceBusy || editor.whiteBalancePicking
                                Layout.fillWidth: true
                                BusyIndicator { running: editor.whiteBalanceBusy; visible: running; implicitWidth: 22; implicitHeight: 22 }
                                Label {
                                    text: editor.whiteBalanceBusy ? qsTranslate("Apertone", "Estimating white balance…") : qsTranslate("Apertone", "Select a neutral area")
                                    Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 11; color: Theme.muted
                                }
                                ApertoneButton { text: qsTranslate("Apertone", "Cancel"); onClicked: editor.cancelWhiteBalance() }
                            }
                            Label {
                                objectName: "develop.whiteBalance.message"
                                visible: text.length > 0; text: editor.whiteBalanceMessage
                                Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 11; color: Theme.accent
                            }
                            DevelopSlider { parameter: "temperature"; label: qsTranslate("Apertone", "Temperature"); minimum: 2000; maximum: 25000; step: 50; defaultValue: 6500; suffix: " K"; correction: "whiteBalance"; allowDisable: false; Layout.fillWidth: true }
                            DevelopSlider { parameter: "tint"; label: qsTranslate("Apertone", "Tint"); minimum: -150; maximum: 150; correction: "whiteBalance"; allowDisable: false; Layout.fillWidth: true }
                            Label { visible: !editor.customWhiteBalance; text: qsTranslate("Apertone", "As Shot keeps the source white balance. Kelvin is available in Custom."); font.pixelSize: 10; color: Theme.muted; wrapMode: Text.Wrap; Layout.fillWidth: true }
                            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                            Label { text: qsTranslate("Apertone", "LIGHT"); color: Theme.muted; font.pixelSize: 9; font.letterSpacing: 2 }
                            DevelopSlider { parameter: "exposure"; label: qsTranslate("Apertone", "Exposure"); minimum: -5; maximum: 5; step: 0.01; suffix: " EV"; Layout.fillWidth: true }
                            DevelopSlider { parameter: "contrast"; label: qsTranslate("Apertone", "Contrast"); Layout.fillWidth: true }
                            DevelopSlider { parameter: "highlights"; label: qsTranslate("Apertone", "Highlights"); Layout.fillWidth: true }
                            DevelopSlider { parameter: "shadows"; label: qsTranslate("Apertone", "Shadows"); Layout.fillWidth: true }
                            DevelopSlider { parameter: "whites"; label: qsTranslate("Apertone", "Whites"); Layout.fillWidth: true }
                            DevelopSlider { parameter: "blacks"; label: qsTranslate("Apertone", "Blacks"); Layout.fillWidth: true }
                            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                            Label { text: qsTranslate("Apertone", "COLOR"); color: Theme.muted; font.pixelSize: 9; font.letterSpacing: 2 }
                            DevelopSlider { parameter: "vibrance"; label: qsTranslate("Apertone", "Vibrance"); Layout.fillWidth: true }
                            DevelopSlider { parameter: "saturation"; label: qsTranslate("Apertone", "Saturation"); Layout.fillWidth: true }
                            Label { text: qsTranslate("Apertone", "Presence"); color: Theme.muted; font.pixelSize: 10 }
                            CreativePanel { section: 256; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        }
                        ApertoneComboBox {
                            objectName: "develop.sectionPicker"
                            Layout.fillWidth: true
                            displayText: qsTranslate("Apertone", "Adjustments")
                            Accessible.name: displayText
                            model: adjustmentsScroll.sections.map(section => section.title)
                            onActivated: function(index) { adjustmentsScroll.revealSection(adjustmentsScroll.sections[index]) }
                        }
                        LocalPanel { id: maskPanel; Layout.fillWidth: true }
                        DevelopSection {
                            id: curveSection
                            objectName: "develop.section.curve"
                            title: qsTranslate("Apertone", "Curve")
                            CurvesPanel { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: adjustmentsScroll.availableWidth }
                        }
                        DevelopSection {
                            id: colorSection
                            objectName: "develop.section.colorMix"
                            title: qsTranslate("Apertone", "Color Mixer")
                            HSLPanel { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: adjustmentsScroll.availableWidth }
                        }
                        DevelopSection {
                            id: pointColorSection
                            objectName: "develop.section.pointColor"
                            title: qsTranslate("Apertone", "Point Color")
                            expanded: false
                            CreativePanel { section: 512; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        }
                        DevelopSection {
                            id: gradingSection
                            objectName: "develop.section.grading"
                            title: qsTranslate("Apertone", "Color Grading")
                            expanded: false
                            CreativePanel { section: 1024; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        }
                        DevelopSection {
                            id: blackWhiteSection
                            objectName: "develop.section.blackWhite"
                            title: qsTranslate("Apertone", "Black and White")
                            expanded: false
                            CreativePanel { section: 4096; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        }
                        DevelopSection {
                            id: detailSection
                            objectName: "develop.section.detail"
                            title: qsTranslate("Apertone", "Detail")
                            DetailPanel { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: adjustmentsScroll.availableWidth }
                        }
                        DevelopSection {
                            id: opticsSection
                            objectName: "develop.section.optics"
                            title: qsTranslate("Apertone", "Optics")
                            LensPanel { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: adjustmentsScroll.availableWidth }
                            Label { text: qsTranslate("Apertone", "Defringe"); color: Theme.muted; font.pixelSize: 10 }
                            CreativePanel { section: 8192; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        }
                        DevelopSection {
                            id: geometrySection
                            objectName: "develop.section.geometry"
                            title: qsTranslate("Apertone", "Geometry")
                            CropPanel { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: adjustmentsScroll.availableWidth }
                            TransformPanel { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: adjustmentsScroll.availableWidth }
                        }
                        DevelopSection {
                            id: effectsSection
                            objectName: "develop.section.effects"
                            title: qsTranslate("Apertone", "Effects")
                            expanded: false
                            CreativePanel { section: 2048; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        }
                        DevelopSection {
                            id: calibrationSection
                            objectName: "develop.section.calibration"
                            title: qsTranslate("Apertone", "Calibration")
                            expanded: false
                            CreativePanel { section: 16384; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        }
                        DevelopSection {
                            id: retouchSection
                            objectName: "develop.section.retouch"
                            title: qsTranslate("Apertone", "Retouch")
                            RetouchPanel { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: adjustmentsScroll.availableWidth }
                        }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                        Label { text: qsTranslate("Apertone", "CAPTURE DATA"); color: Theme.muted; font.pixelSize: 9; font.letterSpacing: 1.5 }
                        MetadataRow { objectName: "develop.metadata.camera"; label: qsTranslate("Apertone", "Camera"); value: editor.camera; Layout.fillWidth: true }
                        MetadataRow { objectName: "develop.metadata.lens"; label: qsTranslate("Apertone", "Lens"); value: { const language = Qt.uiLanguage; return editor.lens } Layout.fillWidth: true }
                        MetadataRow { objectName: "develop.metadata.iso"; label: "ISO"; value: editor.iso; Layout.fillWidth: true }
                        MetadataRow { objectName: "develop.metadata.shutter"; label: qsTranslate("Apertone", "Shutter speed"); value: editor.shutter; Layout.fillWidth: true }
                        MetadataRow { objectName: "develop.metadata.aperture"; label: qsTranslate("Apertone", "Aperture"); value: editor.aperture; Layout.fillWidth: true }
                        MetadataRow { objectName: "develop.metadata.focalLength"; label: qsTranslate("Apertone", "Focal length"); value: editor.focalLength; Layout.fillWidth: true }
                        Label { text: editor.saving ? qsTranslate("Apertone", "Saving changes…") : qsTranslate("Apertone", "Changes are saved automatically"); color: Theme.muted; font.pixelSize: 10; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    }
                }
            }
        }
        Rectangle {
            objectName: "main.footer"
            Layout.fillWidth: true; implicitHeight: Theme.footerHeight; color: Theme.panel
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 6
                FooterCommand {
                    objectName: "openPhotoButton"
                    label: qsTranslate("Apertone", "Open photo")
                    fullText: qsTranslate("Apertone", "+  Open photo")
                    compactText: "+"
                    enabled: !editor.busy
                    onClicked: fileDialog.open()
                }
                Item {
                    id: footerImportSlot
                    visible: root.compactWorkspace && root.libraryMode && !root.workspaceLeftVisible
                    implicitWidth: libraryPane.importButtonWidth; implicitHeight: Theme.controlHeight
                }
                Item {
                    id: footerExportSlot
                    visible: !root.libraryMode || !root.workspaceLeftVisible
                    implicitWidth: exportButton.implicitWidth; implicitHeight: Theme.controlHeight
                }
                Rectangle { implicitWidth: 1; implicitHeight: 16; color: Theme.divider }
                FooterCommand {
                    objectName: "main.undoButton"
                    label: qsTranslate("Apertone", "Undo")
                    compactText: "↶"
                    enabled: !editor.busy && !batch.working && (root.libraryMode ? batch.canUndo : editor.canUndo || batch.canUndo)
                    onClicked: root.libraryMode || !editor.canUndo ? batch.undo() : editor.undo()
                }
                FooterCommand {
                    objectName: "main.redoButton"
                    label: qsTranslate("Apertone", "Redo")
                    compactText: "↷"
                    enabled: !editor.busy && !batch.working && (root.libraryMode ? batch.canRedo : editor.canRedo || batch.canRedo)
                    onClicked: root.libraryMode || !editor.canRedo ? batch.redo() : editor.redo()
                }
                Label { objectName: "main.filename"; Layout.fillWidth: true; Layout.minimumWidth: 0; text: root.libraryMode ? (library.selectedPhotoSummary.filename || "") : editor.filename; textFormat: Text.PlainText; color: Theme.text; font.pixelSize: 11; elide: Text.ElideMiddle }
                Rectangle { width: 5; height: 5; radius: 3; color: editor.busy || editor.saving ? Theme.accent : Theme.positive }
                Label { objectName: "main.editorStatus"; Layout.minimumWidth: 0; text: qsTranslate("Apertone", editor.status); font.pixelSize: 10; color: Theme.muted; Layout.fillWidth: true; elide: Text.ElideRight }
                Label { objectName: "applicationVersion"; visible: !root.compactWorkspace || root.width >= 700; text: "Apertone " + Qt.application.version; font.pixelSize: 10; color: Theme.muted }
                ToolButton {
                    objectName: "main.workspaceView"
                    text: qsTranslate("Apertone", "View")
                    Layout.preferredHeight: Theme.footerHeight
                    onClicked: workspaceMenu.open()
                    Menu {
                        id: workspaceMenu
                        y: -height
                        MenuItem { text: qsTranslate("Apertone", "Workspace header") + "  F5"; checkable: true; checked: root.workspaceHeaderVisible; onTriggered: root.workspaceHeaderVisible = checked }
                        MenuItem { text: qsTranslate("Apertone", "Filmstrip") + "  F6"; checkable: true; checked: root.filmstripVisible; enabled: root.hasFilmstrip; onTriggered: root.filmstripVisible = checked }
                        MenuItem { text: qsTranslate("Apertone", "Left panel") + "  F7"; checkable: true; checked: root.workspaceLeftVisible; onTriggered: root.setPanelVisible(true, checked) }
                        MenuItem { text: qsTranslate("Apertone", "Right panel") + "  F8"; checkable: true; checked: root.developRightVisible; onTriggered: root.setPanelVisible(false, checked) }
                        MenuItem { text: qsTranslate("Apertone", "View tools") + "  T"; checkable: true; checked: root.workspaceToolsVisible; onTriggered: root.workspaceToolsVisible = checked }
                    }
                }
            }
        }
        ListView {
            id: filmstrip
            objectName: root.libraryMode ? "library.filmstrip" : "develop.filmstrip"
            visible: root.filmstripVisible && root.hasFilmstrip
            Layout.fillWidth: true; Layout.preferredHeight: visible ? root.stripHeight : 0
            orientation: ListView.Horizontal; clip: true; spacing: 1; model: library; cacheBuffer: 300
            function revealCurrentPhoto() {
                if (!visible) return
                const row = !root.libraryMode ? library.reviewPhotoRow(editor.photoID) :
                            libraryPane.viewMode === "loupe" ? libraryPane.loupeRow : libraryPane.currentGridRow
                if (row < 0 || row >= count || row === currentIndex) return
                currentIndex = row
                positionViewAtIndex(row, ListView.Contain)
            }
            onVisibleChanged: if (visible) Qt.callLater(revealCurrentPhoto)
            Connections {
                target: libraryPane
                function onLoupeRowChanged() { if (root.libraryMode) Qt.callLater(filmstrip.revealCurrentPhoto) }
                function onCurrentGridRowChanged() { if (root.libraryMode) Qt.callLater(filmstrip.revealCurrentPhoto) }
            }
            Connections {
                target: root
                function onLibraryModeChanged() { Qt.callLater(filmstrip.revealCurrentPhoto) }
            }
            function moveCurrentPhoto(direction, event) {
                event.accepted = !root.libraryMode && event.modifiers === Qt.NoModifier
                if (!event.accepted) return
                if (direction > 0) incrementCurrentIndex()
                else decrementCurrentIndex()
                // Mouse clicks retain their viewport. Keyboard navigation must
                // explicitly reveal its item even after that positioning.
                if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
            }
            Keys.onRightPressed: function(event) { moveCurrentPhoto(1, event) }
            Keys.onLeftPressed: function(event) { moveCurrentPhoto(-1, event) }
            delegate: PhotoCard {
                id: stripCard
                width: Math.round(root.stripHeight * 112 / Theme.filmstripHeight); height: root.stripHeight; compact: true
                reviewPhoto: !root.libraryMode ? null : function(assetID) {
                    if (libraryPane.viewMode === "loupe") libraryPane.showLoupe(assetID)
                    else libraryPane.revealGridRow(stripCard.index)
                }
            }
            ScrollBar.horizontal: ScrollBar { objectName: root.libraryMode ? "library.filmstripBar" : "develop.filmstripBar" }
        }
    }
    ApertoneButton {
        id: exportButton
        objectName: "exportButton"
        parent: root.libraryMode && root.workspaceLeftVisible ? libraryPane.exportSlot : footerExportSlot
        anchors.fill: parent
        text: qsTranslate("Apertone", "Export")
        enabled: (editor.hasPhoto || library.selectedCount > 0) && !editor.busy && !editor.exporting
        onClicked: exportSettings.open()
    }
}
