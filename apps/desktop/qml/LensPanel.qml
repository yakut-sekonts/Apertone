import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    objectName: "lensPanel"
    spacing: 8
    property var state: editor.lensState
    property var options: editor.lensOptions
    RowLayout {
        Layout.fillWidth: true
        Label { text: "LENS CORRECTIONS"; color: "#858c97"; font.pixelSize: 10; font.letterSpacing: 1.5; Layout.fillWidth: true }
        CheckBox { objectName: "lensEnabled"; checked: editor.correctionStates.lens; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setCorrectionEnabled("lens", checked); Accessible.name: "Включить Lens Corrections" }
    }
    TextField { id: search; objectName: "lensSearch"; placeholderText: "Найти профиль объектива"; Layout.fillWidth: true; enabled: editor.hasPhoto && !editor.busy; Accessible.name: "Поиск профиля объектива" }
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
        Accessible.name: "Профиль объектива"
    }
    Label { text: panel.state.resolvedProfile || "—"; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#d9dde4"; font.pixelSize: 11 }
    Label { objectName: "lensStatus"; text: panel.state.status || "Откройте RAW"; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#858c97"; font.pixelSize: 10 }
    DevelopSlider { objectName: "lensDistortion"; parameter: "lens.distortion"; label: "Distortion"; minimum: 0; maximum: 200; defaultValue: 100; correction: "lens"; allowDisable: false; enabled: panel.state.distortionAvailable || false }
    DevelopSlider { objectName: "lensVignetting"; parameter: "lens.vignetting"; label: "Vignetting"; minimum: 0; maximum: 200; defaultValue: 100; correction: "lens"; allowDisable: false; enabled: panel.state.vignettingAvailable || false }
    CheckBox { objectName: "lensCA"; text: "Chromatic aberration"; checked: !!editor.parameters["lens.chromaticAberration"]; enabled: editor.hasPhoto && !editor.busy && !!panel.state.caAvailable; onToggled: editor.setParameter("lens.chromaticAberration", checked ? 1 : 0) }
    Label { text: "100 — калиброванная коррекция. Геометрия может немного уменьшить поле зрения."; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 10; color: "#858c97" }
    Button { objectName: "lensReset"; text: "Сбросить Lens Corrections"; enabled: editor.hasPhoto && !editor.busy; Layout.fillWidth: true; onClicked: editor.resetLens() }
}
