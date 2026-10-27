import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: panel
    property int section: sectionPicker.currentValue ?? 256
    property string gradingChannel: wheelPicker.currentValue ?? "shadows"
    spacing: 8
    ApertoneButton {
        objectName: "develop.autoAdjust"
        text: qsTranslate("Apertone", "Auto Adjust"); Layout.fillWidth: true
        enabled: editor.hasPhoto && !editor.busy && !editor.original && !editor.creativeBusy
        onClicked: editor.autoAdjust()
    }
    ApertoneComboBox {
        id: sectionPicker; Layout.fillWidth: true; textRole: "label"; valueRole: "value"
        model: [{value:256,label:qsTranslate("Apertone","Presence")},{value:512,label:qsTranslate("Apertone","Point Color")},
                {value:1024,label:qsTranslate("Apertone","Color Grading")},{value:2048,label:qsTranslate("Apertone","Effects")}]
    }
    RowLayout {
        visible: panel.section===512; Layout.fillWidth: true
        ApertoneButton {
            text: qsTranslate("Apertone","Eyedropper"); checkable: true; checked: editor.pointColorPicking
            enabled: editor.hasPhoto && !editor.busy && !editor.original && !editor.creativeBusy
            onClicked: editor.pointColorPicking=!editor.pointColorPicking
        }
        CheckBox { text: qsTranslate("Apertone","Visualize range"); checked:editor.pointColorRange; onToggled:editor.pointColorRange=checked }
    }
    RowLayout {
        visible: editor.creativeBusy || editor.pointColorPicking; Layout.fillWidth: true
        BusyIndicator { running:editor.creativeBusy; visible:running; implicitWidth:22; implicitHeight:22 }
        Label { text:qsTranslate("Apertone","Select a color in the photo"); visible:editor.pointColorPicking; wrapMode:Text.Wrap; Layout.fillWidth:true }
        ApertoneButton { text:qsTranslate("Apertone","Cancel");onClicked:editor.cancelCreative() }
    }
    ApertoneComboBox {
        id: wheelPicker; visible:panel.section===1024; Layout.fillWidth:true; textRole:"label";valueRole:"value"
        model:[{value:"shadows",label:qsTranslate("Apertone","Shadows")},{value:"midtones",label:qsTranslate("Apertone","Midtones")},
               {value:"highlights",label:qsTranslate("Apertone","Highlights")},{value:"global",label:qsTranslate("Apertone","Global")}]
    }
    ColorWheel { visible:panel.section===1024; channel:panel.gradingChannel; Layout.alignment:Qt.AlignHCenter }
    Repeater {
        model:editor.creativeParameters
        delegate:ColumnLayout {
            required property var modelData
            visible:modelData.group===panel.section &&
                (!modelData.name.startsWith("grading.") || modelData.name==="grading.balance" || modelData.name==="grading.blending" ||
                 modelData.name.startsWith("grading."+panel.gradingChannel+"."))
            Layout.fillWidth:true
            Label {
                visible:modelData.name==="vignette.amount" || modelData.name==="grain.amount"
                text:modelData.name==="grain.amount"?qsTranslate("Apertone","Grain"):qsTranslate("Apertone","Post Crop Vignette")
                color:Theme.text;font.bold:true
            }
            DevelopSlider {
                parameter:modelData.name;label:qsTranslate("Apertone",modelData.label)
                minimum:modelData.minimum;maximum:modelData.maximum;defaultValue:modelData.initial
                correction:panel.section===256?"presence":panel.section===512?"pointColor":panel.section===1024?"grading":modelData.name.startsWith("grain.")?"grain":"vignette"
                Layout.fillWidth:true
            }
        }
    }
}
