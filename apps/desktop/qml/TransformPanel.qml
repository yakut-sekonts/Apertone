import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    objectName: "transformPanel"
    spacing: 8
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTranslate("Apertone", "TRANSFORM"); color: Theme.muted; font.pixelSize: 10; font.letterSpacing: 1.5; Layout.fillWidth: true }
        CheckBox { objectName: "transformEnabled"; checked: editor.correctionStates.transform; enabled: editor.hasPhoto && !editor.busy; onToggled: editor.setCorrectionEnabled("transform", checked); Accessible.name: qsTranslate("Apertone", "Enable Transform") }
    }
    RowLayout {
        Layout.fillWidth: true
        ApertoneButton { objectName: "transformAuto"; text: editor.transformAutoBusy ? qsTranslate("Apertone", "Finding lines…") : qsTranslate("Apertone", "Auto"); enabled: editor.hasPhoto && !editor.busy && !editor.original && !editor.transformAutoBusy; Layout.fillWidth: true; onClicked: editor.autoTransform() }
        ApertoneButton { objectName: "transformAutoCancel"; visible: editor.transformAutoBusy; text: qsTranslate("Apertone", "Cancel"); onClicked: editor.cancelAutoTransform() }
        ApertoneButton { objectName: "transformGuided"; text: qsTranslate("Apertone", "Guided"); checkable: true; checked: editor.guidedMode; enabled: editor.hasPhoto && !editor.busy && !editor.original; Layout.fillWidth: true; onToggled: editor.guidedMode = checked }
    }
    ColumnLayout {
        visible: editor.guidedMode
        Layout.fillWidth: true
        Label { text: qsTranslate("Apertone", "Align guides with straight lines in the original photo."); wrapMode: Text.Wrap; color: Theme.text; font.pixelSize: 11; Layout.fillWidth: true }
        ApertoneComboBox { objectName: "transformGuideLayout"; Layout.fillWidth: true; model: [qsTranslate("Apertone", "2 vertical"), qsTranslate("Apertone", "2 horizontal"), qsTranslate("Apertone", "Vertical + horizontal")]; currentIndex: editor.transformGuideLayout; onActivated: function(index) { editor.setGuideLayout(index) }; Accessible.name: qsTranslate("Apertone", "Guided correction directions") }
        ApertoneButton { objectName: "transformGuideApply"; text: qsTranslate("Apertone", "Apply guides"); enabled: editor.guidedCanvasReady; Layout.fillWidth: true; onClicked: editor.applyGuides() }
    }
    ColumnLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.maximumWidth: panel.width
        enabled: !editor.guidedMode
        DevelopSlider { objectName: "transformRotate"; parameter: "transform.rotate"; label: qsTranslate("Apertone", "Rotate"); minimum: -45; maximum: 45; step: 0.05; correction: "transform"; allowDisable: false }
        DevelopSlider { objectName: "transformHorizontal"; parameter: "transform.horizontal"; label: qsTranslate("Apertone", "Horizontal"); minimum: -100; maximum: 100; correction: "transform"; allowDisable: false }
        DevelopSlider { objectName: "transformVertical"; parameter: "transform.vertical"; label: qsTranslate("Apertone", "Vertical"); minimum: -100; maximum: 100; correction: "transform"; allowDisable: false }
        DevelopSlider { objectName: "transformAspect"; parameter: "transform.aspect"; label: qsTranslate("Apertone", "Aspect"); minimum: -100; maximum: 100; correction: "transform"; allowDisable: false }
        DevelopSlider { objectName: "transformScale"; parameter: "transform.scale"; label: qsTranslate("Apertone", "Scale"); minimum: 50; maximum: 200; defaultValue: 100; correction: "transform"; allowDisable: false }
        DevelopSlider { objectName: "transformXOffset"; parameter: "transform.xOffset"; label: qsTranslate("Apertone", "X Offset"); minimum: -100; maximum: 100; correction: "transform"; allowDisable: false }
        DevelopSlider { objectName: "transformYOffset"; parameter: "transform.yOffset"; label: qsTranslate("Apertone", "Y Offset"); minimum: -100; maximum: 100; correction: "transform"; allowDisable: false }
        CheckBox {
            id: constrain
            objectName: "transformConstrain"
            Layout.fillWidth: true; Layout.minimumWidth: 0
            text: qsTranslate("Apertone", "Constrain Crop · remove empty edges")
            checked: !!editor.parameters["transform.constrainCrop"]
            enabled: editor.hasPhoto && !editor.busy
            onToggled: editor.setParameter("transform.constrainCrop", checked ? 1 : 0)
            contentItem: Label {
                text: constrain.text; font: constrain.font; color: constrain.palette.buttonText
                verticalAlignment: Text.AlignVCenter; wrapMode: Text.Wrap
                leftPadding: constrain.indicator && !constrain.mirrored ? constrain.indicator.width + constrain.spacing : 0
                rightPadding: constrain.indicator && constrain.mirrored ? constrain.indicator.width + constrain.spacing : 0
            }
        }
    }
    Label { visible: !!editor.transformState.guided; text: qsTranslate("Apertone", "Auto/Guided matrix applied; manual controls adjust it further."); wrapMode: Text.Wrap; Layout.fillWidth: true; color: Theme.text; font.pixelSize: 10 }
    Label { text: qsTranslate("Apertone", "Actual scale: ") + Number(editor.transformState.effectiveScale || 100).toFixed(1) + "%"; color: Theme.muted; font.pixelSize: 10 }
    Label { text: qsTranslate("Apertone", "Auto and Guided replace Transform in one Undo step. Crop is preserved. Unconstrained edges are transparent; JPEG uses black."); wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 10; color: Theme.muted }
    ApertoneButton { objectName: "transformReset"; text: qsTranslate("Apertone", "Reset Transform"); enabled: editor.hasPhoto && !editor.busy; Layout.fillWidth: true; onClicked: editor.resetTransform() }
}
