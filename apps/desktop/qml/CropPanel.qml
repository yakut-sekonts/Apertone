import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    objectName: "cropPanel"
    spacing: 8
    property bool available: editor.hasPhoto && !editor.busy && editor.correctionStates.crop
    RowLayout {
        Layout.fillWidth: true
        Label { text: "CROP / STRAIGHTEN"; color: "#858c97"; font.pixelSize: 10; font.letterSpacing: 1.5; Layout.fillWidth: true }
        CheckBox { objectName: "cropEnabled"; checked: editor.correctionStates.crop; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setCorrectionEnabled("crop", checked); Accessible.name: "Включить Crop" }
    }
    Button { objectName: "cropTool"; text: editor.cropMode ? "Готово" : "Изменить рамку"; highlighted: editor.cropMode; enabled: panel.available && !editor.original; Layout.fillWidth: true; onClicked: editor.cropMode = !editor.cropMode }
    ComboBox {
        id: aspectBox
        objectName: "cropAspect"
        Layout.fillWidth: true; enabled: panel.available
        model: ["Free", "Original", "1:1", "4:3", "3:2", "16:9", "Custom"]
        currentIndex: {
            const a = editor.cropState.aspect ?? 0
            if (a === 0) return 0
            if (a === -1) return 1
            const ratios = [1, 4/3, 3/2, 16/9]
            for (let i=0;i<ratios.length;i++) if (Math.abs(a-ratios[i])<0.000001) return i+2
            return 6
        }
        onActivated: function(index) {
            const ratios = [0,-1,1,4/3,3/2,16/9]
            if (index<6) editor.setParameter("crop.aspect", ratios[index])
            else panel.applyCustomRatio()
        }
        Accessible.name: "Соотношение сторон"
    }
    function applyCustomRatio() {
        if (panel.validCustomRatio) {
            const value = Number(ratioWidth.text) / Number(ratioHeight.text)
            if (value>=0.01 && value<=100) editor.setParameter("crop.aspect", value)
        }
    }
    readonly property real customRatio: Number(ratioWidth.text) / Number(ratioHeight.text)
    readonly property bool validCustomRatio: ratioWidth.acceptableInput && ratioHeight.acceptableInput && customRatio >= 0.01 && customRatio <= 100
    RowLayout {
        Layout.fillWidth: true
        TextField { id: ratioWidth; objectName: "cropRatioWidth"; text: "5"; placeholderText: "Width"; enabled: panel.available; Layout.fillWidth: true; validator: DoubleValidator {bottom: 0.01; top: 1000; locale: "C"} onAccepted: panel.applyCustomRatio(); Accessible.name: "Custom ratio width" }
        Label { text: ":" }
        TextField { id: ratioHeight; objectName: "cropRatioHeight"; text: "4"; placeholderText: "Height"; enabled: panel.available; Layout.fillWidth: true; validator: DoubleValidator {bottom: 0.01; top: 1000; locale: "C"} onAccepted: panel.applyCustomRatio(); Accessible.name: "Custom ratio height" }
        Button { objectName: "cropCustomApply"; text: "✓"; enabled: panel.available && panel.validCustomRatio; onClicked: panel.applyCustomRatio(); Accessible.name: "Применить custom ratio" }
    }
    Button { objectName: "cropAspectSwap"; text: "Поменять стороны ratio"; enabled: panel.available && (editor.cropState.aspect ?? 0)>0; Layout.fillWidth: true; onClicked: editor.setParameter("crop.aspect", 1/editor.cropState.aspect) }
    RowLayout {
        Layout.fillWidth: true
        Button { objectName: "cropRotateLeft"; text: "↶ 90°"; enabled: panel.available; Layout.fillWidth: true; onClicked: editor.rotateCrop(-1) }
        Button { objectName: "cropRotateRight"; text: "↷ 90°"; enabled: panel.available; Layout.fillWidth: true; onClicked: editor.rotateCrop(1) }
    }
    RowLayout {
        CheckBox { objectName: "cropFlipHorizontal"; text: "Flip H"; checked: editor.cropState.flipHorizontal ?? false; enabled: panel.available; onToggled: editor.setParameter("crop.flipHorizontal", checked ? 1 : 0) }
        CheckBox { objectName: "cropFlipVertical"; text: "Flip V"; checked: editor.cropState.flipVertical ?? false; enabled: panel.available; onToggled: editor.setParameter("crop.flipVertical", checked ? 1 : 0) }
    }
    Label { text: "Straighten · " + Number(editor.cropState.angle ?? 0).toFixed(2) + "°"; color: "#d7dce3" }
    Slider { objectName: "cropAngleSlider"; Layout.fillWidth: true; from: -45; to: 45; stepSize: 0.05; value: editor.cropState.angle ?? 0; enabled: panel.available; onPressedChanged: pressed ? editor.beginEdit() : editor.endEdit(); onMoved: editor.setParameter("crop.angle", value); Accessible.name: "Straighten angle" }
    ComboBox { objectName: "cropGuide"; Layout.fillWidth: true; model: ["Thirds", "Grid", "Diagonal", "Golden ratio"]; currentIndex: ["thirds", "grid", "diagonal", "golden"].indexOf(editor.cropOverlay); onActivated: function(index) {editor.cropOverlay=["thirds", "grid", "diagonal", "golden"][index]}; Accessible.name: "Crop overlay" }
    Label { text: editor.hasPhoto ? (editor.cropState.outputWidth + " × " + editor.cropState.outputHeight + " px") : ""; color: "#858c97"; font.pixelSize: 10 }
    Label { text: "Перетаскивайте рамку и её края. Straighten автоматически убирает пустые углы."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#858c97"; font.pixelSize: 10 }
    Button { objectName: "cropReset"; text: "Сбросить Crop"; enabled: editor.hasPhoto && !editor.busy; Layout.fillWidth: true; onClicked: editor.resetCrop() }
}
