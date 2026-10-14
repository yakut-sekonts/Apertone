import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    objectName: "transformPanel"
    spacing: 8
    RowLayout {
        Layout.fillWidth: true
        Label { text: "TRANSFORM"; color: "#858c97"; font.pixelSize: 10; font.letterSpacing: 1.5; Layout.fillWidth: true }
        CheckBox { objectName: "transformEnabled"; checked: editor.correctionStates.transform; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setCorrectionEnabled("transform", checked); Accessible.name: "Включить Transform" }
    }
    RowLayout {
        Layout.fillWidth: true
        Button { objectName: "transformAuto"; text: editor.transformAutoBusy ? "Поиск линий…" : "Auto"; enabled: editor.hasPhoto && !editor.busy && !editor.original && !editor.transformAutoBusy; Layout.fillWidth: true; onClicked: editor.autoTransform() }
        Button { objectName: "transformAutoCancel"; visible: editor.transformAutoBusy; text: "Отменить"; onClicked: editor.cancelAutoTransform() }
        Button { objectName: "transformGuided"; text: "Guided"; checkable: true; checked: editor.guidedMode; enabled: editor.hasPhoto && !editor.busy && !editor.original; Layout.fillWidth: true; onToggled: editor.guidedMode = checked }
    }
    ColumnLayout {
        visible: editor.guidedMode
        Layout.fillWidth: true
        Label { text: "Совместите направляющие с прямыми линиями исходного кадра."; wrapMode: Text.Wrap; color: "#c9d2dd"; font.pixelSize: 11; Layout.fillWidth: true }
        ComboBox { objectName: "transformGuideLayout"; Layout.fillWidth: true; model: ["2 vertical", "2 horizontal", "Vertical + horizontal"]; onActivated: function(index) { editor.setGuideLayout(index) }; Accessible.name: "Направления Guided correction" }
        Button { objectName: "transformGuideApply"; text: "Применить направляющие"; enabled: editor.guidedCanvasReady; Layout.fillWidth: true; onClicked: editor.applyGuides() }
    }
    ColumnLayout {
        Layout.fillWidth: true
        enabled: !editor.guidedMode
        DevelopSlider { objectName: "transformRotate"; parameter: "transform.rotate"; label: "Rotate"; minimum: -45; maximum: 45; step: 0.05; correction: "transform"; allowDisable: false }
        DevelopSlider { objectName: "transformHorizontal"; parameter: "transform.horizontal"; label: "Horizontal"; minimum: -100; maximum: 100; correction: "transform"; allowDisable: false }
        DevelopSlider { objectName: "transformVertical"; parameter: "transform.vertical"; label: "Vertical"; minimum: -100; maximum: 100; correction: "transform"; allowDisable: false }
        DevelopSlider { objectName: "transformAspect"; parameter: "transform.aspect"; label: "Aspect"; minimum: -100; maximum: 100; correction: "transform"; allowDisable: false }
        DevelopSlider { objectName: "transformScale"; parameter: "transform.scale"; label: "Scale"; minimum: 50; maximum: 200; defaultValue: 100; correction: "transform"; allowDisable: false }
        DevelopSlider { objectName: "transformXOffset"; parameter: "transform.xOffset"; label: "X Offset"; minimum: -100; maximum: 100; correction: "transform"; allowDisable: false }
        DevelopSlider { objectName: "transformYOffset"; parameter: "transform.yOffset"; label: "Y Offset"; minimum: -100; maximum: 100; correction: "transform"; allowDisable: false }
        CheckBox { objectName: "transformConstrain"; text: "Constrain Crop · убрать пустые края"; checked: !!editor.parameters["transform.constrainCrop"]; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setParameter("transform.constrainCrop", checked ? 1 : 0) }
    }
    Label { visible: !!editor.transformState.guided; text: "Матрица Auto/Guided применена; ручные параметры дополняют её."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#c9d2dd"; font.pixelSize: 10 }
    Label { text: "Фактический Scale: " + Number(editor.transformState.effectiveScale || 100).toFixed(1) + "%"; color: "#858c97"; font.pixelSize: 10 }
    Label { text: "Auto и Guided заменяют Transform одной операцией Undo. Crop сохраняется. Без Constrain пустые края прозрачны; JPEG — чёрные."; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 10; color: "#858c97" }
    Button { objectName: "transformReset"; text: "Сбросить Transform"; enabled: editor.hasPhoto && !editor.busy; Layout.fillWidth: true; onClicked: editor.resetTransform() }
}
