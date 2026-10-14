import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    objectName: "lensPanel"
    spacing: 8
    property var state: { const language = Qt.uiLanguage; return editor.lensState }
    property var options: editor.lensOptions.map(function(item) { return item.id ? item : Object.assign({}, item, {name: qsTranslate("Luma", item.name)}) })
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTranslate("Luma", "LENS CORRECTIONS"); color: "#858c97"; font.pixelSize: 10; font.letterSpacing: 1.5; Layout.fillWidth: true }
        CheckBox { objectName: "lensEnabled"; checked: editor.correctionStates.lens; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setCorrectionEnabled("lens", checked); Accessible.name: qsTranslate("Luma", "Enable lens corrections") }
    }
    TextField { id: search; objectName: "lensSearch"; placeholderText: qsTranslate("Luma", "Find lens profile"); Layout.fillWidth: true; enabled: editor.hasPhoto && !editor.busy; Accessible.name: qsTranslate("Luma", "Lens profile search") }
    ComboBox {
        id: profiles
        objectName: "lensProfile"
        Layout.fillWidth: true
        enabled: editor.hasPhoto && !editor.busy
        textRole: "name"
        valueRole: "id"
        model: panel.options.filter(function(item) { return !item.id || item.id === panel.state.profile || item.name.toLowerCase().includes(search.text.toLowerCase()) })
        currentIndex: {
            for (let i = 0; i < model.length; ++i)
                if (model[i].id === panel.state.profile) return i
            return 0
        }
        onActivated: function(index) { editor.setLensProfile(model[index].id) }
        Accessible.name: qsTranslate("Luma", "Lens profile")
    }
    Label { text: panel.state.resolvedProfile || "—"; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#d9dde4"; font.pixelSize: 11 }
    Label { objectName: "lensStatus"; text: qsTranslate("Luma", panel.state.status || "") || qsTranslate("Luma", "Open a photo"); wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#858c97"; font.pixelSize: 10 }
    DevelopSlider { objectName: "lensDistortion"; parameter: "lens.distortion"; label: qsTranslate("Luma", "Distortion"); minimum: 0; maximum: 200; defaultValue: 100; correction: "lens"; allowDisable: false; enabled: panel.state.distortionAvailable || false }
    DevelopSlider { objectName: "lensVignetting"; parameter: "lens.vignetting"; label: qsTranslate("Luma", "Vignetting"); minimum: 0; maximum: 200; defaultValue: 100; correction: "lens"; allowDisable: false; enabled: panel.state.vignettingAvailable || false }
    CheckBox { objectName: "lensCA"; text: qsTranslate("Luma", "Chromatic aberration"); checked: !!editor.parameters["lens.chromaticAberration"]; enabled: editor.hasPhoto && !editor.busy && !!panel.state.caAvailable; onToggled: editor.setParameter("lens.chromaticAberration", checked ? 1 : 0) }
    Label { text: qsTranslate("Luma", "100 is the calibrated correction. Geometry may slightly reduce the field of view."); wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 10; color: "#858c97" }
    Button { objectName: "lensReset"; text: qsTranslate("Luma", "Reset lens corrections"); enabled: editor.hasPhoto && !editor.busy; Layout.fillWidth: true; onClicked: editor.resetLens() }
}
