import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    objectName: "detailPanel"
    spacing: 8
    RowLayout {
        Layout.fillWidth: true
        Label { text: "DETAIL / SHARPENING"; color: "#858c97"; font.pixelSize: 10; font.letterSpacing: 1.5; Layout.fillWidth: true }
        CheckBox { objectName: "sharpenEnabled"; checked: editor.correctionStates.sharpening; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setCorrectionEnabled("sharpening", checked); Accessible.name: "Включить Sharpening" }
    }
    DevelopSlider { objectName: "sharpenAmount"; parameter: "sharpen.amount"; label: "Amount"; minimum: 0; maximum: 150; correction: "sharpening"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "sharpenRadius"; parameter: "sharpen.radius"; label: "Radius"; minimum: 0.5; maximum: 3; step: 0.05; defaultValue: 1; correction: "sharpening"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "sharpenDetail"; parameter: "sharpen.detail"; label: "Detail"; minimum: 0; maximum: 100; defaultValue: 25; correction: "sharpening"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "sharpenMasking"; parameter: "sharpen.masking"; label: "Masking"; minimum: 0; maximum: 100; correction: "sharpening"; allowDisable: false; detailDiagnostic: true }
    RowLayout {
        Layout.fillWidth: true
        Label { text: "LUMINANCE NOISE"; color: "#858c97"; font.pixelSize: 10; Layout.fillWidth: true }
        CheckBox { objectName: "luminanceNoiseEnabled"; checked: editor.correctionStates.luminanceNoise; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setCorrectionEnabled("luminanceNoise", checked); Accessible.name: "Включить LUMINANCE NOISE" }
    }
    DevelopSlider { objectName: "noiseluminance"; parameter: "noise.luminance"; label: "Amount"; minimum: 0; maximum: 100; defaultValue: 0; correction: "luminanceNoise"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "noiseluminanceDetail"; parameter: "noise.luminanceDetail"; label: "Detail"; minimum: 0; maximum: 100; defaultValue: 50; correction: "luminanceNoise"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "noiseluminanceContrast"; parameter: "noise.luminanceContrast"; label: "Contrast"; minimum: 0; maximum: 100; defaultValue: 0; correction: "luminanceNoise"; allowDisable: false; detailDiagnostic: true }
    RowLayout {
        Layout.fillWidth: true
        Label { text: "COLOR NOISE"; color: "#858c97"; font.pixelSize: 10; Layout.fillWidth: true }
        CheckBox { objectName: "colorNoiseEnabled"; checked: editor.correctionStates.colorNoise; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setCorrectionEnabled("colorNoise", checked); Accessible.name: "Включить COLOR NOISE" }
    }
    DevelopSlider { objectName: "noisecolor"; parameter: "noise.color"; label: "Amount"; minimum: 0; maximum: 100; defaultValue: 0; correction: "colorNoise"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "noisecolorDetail"; parameter: "noise.colorDetail"; label: "Detail"; minimum: 0; maximum: 100; defaultValue: 50; correction: "colorNoise"; allowDisable: false; detailDiagnostic: true }
    DevelopSlider { objectName: "noisecolorSmoothness"; parameter: "noise.colorSmoothness"; label: "Smoothness"; minimum: 0; maximum: 100; defaultValue: 50; correction: "colorNoise"; allowDisable: false; detailDiagnostic: true }
    Label { text: "Option + перетаскивание — диагностика. Radius — пиксели оригинала."; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 10; color: "#858c97" }
}
