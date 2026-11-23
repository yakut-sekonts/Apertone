import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: panel
    required property int section
    property string gradingChannel: wheelPicker.currentValue ?? "shadows"
    spacing: Theme.panelSpacing
    ApertoneCheckBox {
        objectName: panel.section === 4096 ? "develop.blackWhite" : ""
        visible: panel.section === 4096
        text: qsTranslate("Apertone", "Black and white")
        enabled: editor.hasPhoto && !editor.busy && !editor.original
        checked: (editor.parameters["bw.active"] ?? 0) > 0.5
        onToggled: editor.setParameter("bw.active", checked ? 1 : 0)
    }
    RowLayout {
        visible: panel.section===512; Layout.fillWidth: true
        ApertoneButton {
            text: qsTranslate("Apertone","Eyedropper"); checkable: true; checked: editor.pointColorPicking
            enabled: editor.hasPhoto && !editor.busy && !editor.original && !editor.creativeBusy
            onClicked: editor.pointColorPicking=!editor.pointColorPicking
        }
        ApertoneCheckBox { text: qsTranslate("Apertone","Visualize range"); checked:editor.pointColorRange; onToggled:editor.pointColorRange=checked }
    }
    RowLayout {
        visible: panel.section === 512 ? editor.pointColorPicking : panel.section === 256 && editor.creativeBusy
        Layout.fillWidth: true
        BusyIndicator { running: panel.section === 256 && editor.creativeBusy; visible: running; implicitWidth: 22; implicitHeight: 22 }
        Label { text:qsTranslate("Apertone","Select a color in the photo"); visible: panel.section === 512; wrapMode:Text.Wrap; Layout.fillWidth:true }
        ApertoneButton { text:qsTranslate("Apertone","Cancel");onClicked:editor.cancelCreative() }
    }
    ApertoneComboBox {
        id: wheelPicker; visible:panel.section===1024; Layout.fillWidth:true; textRole:"label";valueRole:"value"
        model:[{value:"shadows",label:qsTranslate("Apertone","Shadows")},{value:"midtones",label:qsTranslate("Apertone","Midtones")},
               {value:"highlights",label:qsTranslate("Apertone","Highlights")},{value:"global",label:qsTranslate("Apertone","Global")}]
    }
    ColorWheel { visible:panel.section===1024; channel:panel.gradingChannel; Layout.alignment:Qt.AlignHCenter }
    Repeater {
        model: editor.creativeParameters.filter(parameter => parameter.group === panel.section && parameter.name !== "bw.active")
        delegate:ColumnLayout {
            required property var modelData
            visible: panel.section !== 1024 || modelData.name === "grading.balance" ||
                     modelData.name === "grading.blending" || modelData.name.startsWith("grading." + panel.gradingChannel + ".")
            Layout.fillWidth:true
            Label {
                visible:modelData.name==="vignette.amount" || modelData.name==="grain.amount"
                text:modelData.name==="grain.amount"?qsTranslate("Apertone","Grain"):qsTranslate("Apertone","Post Crop Vignette")
                color:Theme.text;font.bold:true
            }
            DevelopSlider {
                parameter:modelData.name;label:qsTranslate("Apertone",modelData.label)
                minimum:modelData.minimum;maximum:modelData.maximum;defaultValue:modelData.initial
                correction: {
                    switch (modelData.group) {
                    case 256: return "presence"
                    case 512: return "pointColor"
                    case 1024: return "grading"
                    case 4096: return "blackWhite"
                    case 8192: return "defringe"
                    case 16384: return "calibration"
                    default: return modelData.name.startsWith("grain.") ? "grain" : "vignette"
                    }
                }
                allowDisable: panel.section !== 4096
                available: editor.hasPhoto && !editor.busy && (panel.section !== 4096 || (editor.parameters["bw.active"] ?? 0) > 0.5)
                Layout.fillWidth:true
            }
        }
    }
}
