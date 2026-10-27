import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    objectName: "detailPanel"
    spacing: 8
    RowLayout {
        Layout.fillWidth: true
        Label { objectName: "detail.heading"; text: qsTranslate("Apertone", "DETAIL / SHARPENING"); color: Theme.muted; font.pixelSize: 10; font.letterSpacing: 1.5; Layout.fillWidth: true }
        CheckBox { objectName: "sharpenEnabled"; checked: editor.correctionStates.sharpening; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setCorrectionEnabled("sharpening", checked); Accessible.name: qsTranslate("Apertone", "Enable sharpening") }
    }
    DevelopSlider { objectName: "sharpenAmount"; parameter: "sharpen.amount"; label: qsTranslate("Apertone", "Amount"); minimum: 0; maximum: 150; correction: "sharpening"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "sharpenRadius"; parameter: "sharpen.radius"; label: qsTranslate("Apertone", "Radius"); minimum: 0.5; maximum: 3; step: 0.05; defaultValue: 1; correction: "sharpening"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "sharpenDetail"; parameter: "sharpen.detail"; label: qsTranslate("Apertone", "Detail"); minimum: 0; maximum: 100; defaultValue: 25; correction: "sharpening"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "sharpenMasking"; parameter: "sharpen.masking"; label: qsTranslate("Apertone", "Masking"); minimum: 0; maximum: 100; correction: "sharpening"; allowDisable: false; detailDiagnostic: true }
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTranslate("Apertone", "LUMINANCE NOISE"); color: Theme.muted; font.pixelSize: 10; Layout.fillWidth: true }
        CheckBox { objectName: "luminanceNoiseEnabled"; checked: editor.correctionStates.luminanceNoise; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setCorrectionEnabled("luminanceNoise", checked); Accessible.name: qsTranslate("Apertone", "Enable luminance noise reduction") }
    }
    DevelopSlider { objectName: "noiseluminance"; parameter: "noise.luminance"; label: qsTranslate("Apertone", "Amount"); minimum: 0; maximum: 100; defaultValue: 0; correction: "luminanceNoise"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "noiseluminanceDetail"; parameter: "noise.luminanceDetail"; label: qsTranslate("Apertone", "Detail"); minimum: 0; maximum: 100; defaultValue: 50; correction: "luminanceNoise"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "noiseluminanceContrast"; parameter: "noise.luminanceContrast"; label: qsTranslate("Apertone", "Contrast"); minimum: 0; maximum: 100; defaultValue: 0; correction: "luminanceNoise"; allowDisable: false; detailDiagnostic: true }
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTranslate("Apertone", "COLOR NOISE"); color: Theme.muted; font.pixelSize: 10; Layout.fillWidth: true }
        CheckBox { objectName: "colorNoiseEnabled"; checked: editor.correctionStates.colorNoise; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setCorrectionEnabled("colorNoise", checked); Accessible.name: qsTranslate("Apertone", "Enable color noise reduction") }
    }
    DevelopSlider { objectName: "noisecolor"; parameter: "noise.color"; label: qsTranslate("Apertone", "Amount"); minimum: 0; maximum: 100; defaultValue: 0; correction: "colorNoise"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "noisecolorDetail"; parameter: "noise.colorDetail"; label: qsTranslate("Apertone", "Detail"); minimum: 0; maximum: 100; defaultValue: 50; correction: "colorNoise"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "noisecolorSmoothness"; parameter: "noise.colorSmoothness"; label: qsTranslate("Apertone", "Smoothness"); minimum: 0; maximum: 100; defaultValue: 50; correction: "colorNoise"; allowDisable: false; detailDiagnostic: true }
    Label { text: qsTranslate("Apertone", "Option + drag shows diagnostics. Radius uses original-image pixels."); wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 10; color: Theme.muted }
    Label { objectName: "denoise.heading"; text: qsTranslate("Apertone", "AI Denoise"); Layout.fillWidth: true }
    Label {
        text: editor.denoiseModelConfigured
            ? qsTranslate("Apertone", "Creates a new unedited Linear DNG from the RAW. Processing stays on this Mac.")
            : qsTranslate("Apertone", "Choose a compatible local model in Settings to use AI Denoise.")
        wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 10; color: Theme.muted
    }
    Slider {
        id: denoiseAmount
        objectName: "denoise.amount"
        from: 1; to: 100; value: 100
        enabled: !editor.aiBusy
        Layout.fillWidth: true
        Accessible.name: qsTranslate("Apertone", "AI Denoise amount")
    }
    ApertoneButton {
        objectName: "denoise.create"
        text: qsTranslate("Apertone", "Create denoised DNG") + " · " + Math.round(denoiseAmount.value) + "%"
        enabled: editor.denoiseModelConfigured && editor.hasPhoto && !editor.busy && !editor.aiBusy && !editor.editingLocked
        Layout.fillWidth: true
        onClicked: editor.denoisePhoto(denoiseAmount.value)
    }
    ProgressBar { objectName: "denoise.progress"; visible: editor.aiBusy && editor.aiOperation === "AI Denoise"; value: editor.aiProgress; Layout.fillWidth: true }
    ApertoneButton { objectName: "denoise.cancel"; visible: editor.aiBusy && editor.aiOperation === "AI Denoise"; text: qsTranslate("Apertone", "Cancel"); onClicked: editor.cancelAI(); Layout.fillWidth: true }
}
