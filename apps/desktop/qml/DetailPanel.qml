import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    objectName: "detailPanel"
    spacing: 8
    readonly property bool derivedBusy: editor.aiBusy &&
        ["Raw Details", "2× Detail", "Lens Blur Recovery", "Color Moire Correction"]
            .indexOf(editor.aiOperation) >= 0
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
    ApertoneButton {
        objectName: "noise.autoProfile"
        text: qsTranslate("Apertone", "Auto noise profile")
        enabled: editor.hasPhoto && !editor.busy && !editor.original && !editor.saving && !editor.editingLocked
        onClicked: editor.autoNoiseProfile()
        Layout.fillWidth: true
        ToolTip.visible: hovered
        ToolTip.text: qsTranslate("Apertone", "Estimates luminance and color noise from this photo, using ISO as a conservative prior.")
    }
    Label { text: qsTranslate("Apertone", "Option + drag shows diagnostics. Radius uses original-image pixels."); wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 10; color: Theme.muted }
    Label { text: qsTranslate("Apertone", "DERIVED DETAIL"); color: Theme.muted; font.pixelSize: 10; font.letterSpacing: 1.5; Layout.fillWidth: true }
    Label {
        text: qsTranslate("Apertone", "Creates a new Float DNG from the saved Develop result. Detail, blur recovery and color moiré correction are deterministic; 2× Detail uses Lanczos-3. Processing stays on this Mac.")
        wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 10; color: Theme.muted
    }
    Slider {
        id: derivedDetailAmount
        objectName: "detailDerived.amount"
        from: 1; to: 100; value: 50
        enabled: !editor.aiBusy
        Layout.fillWidth: true
        Accessible.name: qsTranslate("Apertone", "Detail amount")
    }
    RowLayout {
        Layout.fillWidth: true
        ApertoneButton {
            objectName: "detailDerived.raw"
            text: qsTranslate("Apertone", "Create Raw Details DNG")
            enabled: editor.hasPhoto && editor.decoderDiagnostics.source === "Bayer RAW" && !editor.busy && !editor.saving && !editor.aiBusy && !editor.editingLocked
            onClicked: editor.createRawDetails(derivedDetailAmount.value)
            Layout.fillWidth: true
        }
        ApertoneButton {
            objectName: "detailDerived.2x"
            text: qsTranslate("Apertone", "Create 2× Detail DNG")
            enabled: editor.hasPhoto && !editor.busy && !editor.saving && !editor.aiBusy && !editor.editingLocked
            onClicked: editor.createDetail2x(derivedDetailAmount.value)
            Layout.fillWidth: true
        }
    }
    RowLayout {
        Layout.fillWidth: true
        ApertoneButton {
            objectName: "detailDerived.deblur"
            text: qsTranslate("Apertone", "Recover lens blur DNG")
            enabled: editor.hasPhoto && !editor.busy && !editor.saving && !editor.aiBusy && !editor.editingLocked
            onClicked: editor.createLensBlurRecovery(derivedDetailAmount.value)
            Layout.fillWidth: true
        }
        ApertoneButton {
            objectName: "detailDerived.moire"
            text: qsTranslate("Apertone", "Correct color moiré DNG")
            enabled: editor.hasPhoto && !editor.busy && !editor.saving && !editor.aiBusy && !editor.editingLocked
            onClicked: editor.createColorMoireCorrection(derivedDetailAmount.value)
            Layout.fillWidth: true
        }
    }
    ProgressBar { visible: panel.derivedBusy; value: editor.aiProgress; Layout.fillWidth: true }
    ApertoneButton { visible: panel.derivedBusy; text: qsTranslate("Apertone", "Cancel"); onClicked: editor.cancelAI(); Layout.fillWidth: true }
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
