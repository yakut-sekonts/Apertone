import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    objectName: "localPanel"
    Layout.preferredWidth: 250
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    Layout.maximumWidth: parent.width
    property bool selected: !!editor.localState.id
    readonly property var kinds: ["linear", "radial", "brush", "luminance", "color"]
    readonly property var operations: ["add", "subtract", "intersect"]
    function kindName(kind) {
        switch(kind) {
        case "linear": return qsTranslate("Apertone", "Linear gradient")
        case "radial": return qsTranslate("Apertone", "Radial gradient")
        case "brush": return qsTranslate("Apertone", "Brush")
        case "luminance": return qsTranslate("Apertone", "Luminance range")
        case "color": return qsTranslate("Apertone", "Color range")
        case "bitmap": return qsTranslate("Apertone", "AI selection")
        }
        return ""
    }
    function operationName(operation) {
        return operation === 0 ? qsTranslate("Apertone", "Add") : operation === 1 ? qsTranslate("Apertone", "Subtract") : qsTranslate("Apertone", "Intersect")
    }
    readonly property var kindNames: kinds.map(kind => kindName(kind))
    readonly property var operationNames: [qsTranslate("Apertone", "Add"), qsTranslate("Apertone", "Subtract"), qsTranslate("Apertone", "Intersect")]
    ApertoneButton {
        id: expanded
        objectName: "localPanelToggle"
        text: qsTranslate("Apertone", "Local adjustments")
        checkable: true
        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
        onToggled: if (!checked) editor.localMode = false
    }
    ColumnLayout {
        visible: expanded.checked
        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
        enabled: editor.hasPhoto && !editor.busy && !editor.editingLocked && !editor.original
        ApertoneComboBox {
            id: newKind
            objectName: "localNewKind"
            model: panel.kindNames
            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
            Accessible.name: qsTranslate("Apertone", "Mask type")
        }
        ApertoneButton {
            objectName: "localNewMask"
            text: qsTranslate("Apertone", "New mask")
            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
            onClicked: editor.addLocalMask(panel.kinds[newKind.currentIndex])
        }
        Label {
            text: qsTranslate("Apertone", "Local AI · photos stay on this Mac")
            wrapMode: Text.Wrap; font.pixelSize: 11; color: Theme.muted
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            ApertoneButton { objectName: "localAI.subject"; text: qsTranslate("Apertone", "Subject"); enabled: !editor.aiBusy; Layout.fillWidth: true; onClicked: editor.generateAIMask("subject") }
            ApertoneButton { text: qsTranslate("Apertone", "Background"); enabled: !editor.aiBusy; Layout.fillWidth: true; onClicked: editor.generateAIMask("background") }
        }
        ApertoneButton {
            text: qsTranslate("Apertone", "Select object")
            enabled: !editor.aiBusy
            Layout.fillWidth: true
            onClicked: editor.beginAIObjectSelection()
        }
        ApertoneButton {
            text: qsTranslate("Apertone", "Sky")
            enabled: !editor.aiBusy && editor.skyModelConfigured
            Layout.fillWidth: true
            onClicked: editor.generateAIMask("sky")
            ToolTip.visible: hovered && !editor.skyModelConfigured
            ToolTip.text: qsTranslate("Apertone", "Choose a compatible local Sky model in Settings.")
        }
        Label {
            text: qsTranslate("Apertone", "People details")
            font.pixelSize: 10; font.weight: Font.DemiBold; color: Theme.muted
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            ApertoneButton { objectName: "localAI.face"; text: qsTranslate("Apertone", "Face"); enabled: !editor.aiBusy; Layout.fillWidth: true; onClicked: editor.beginAIMaskSelection("face") }
            ApertoneButton { objectName: "localAI.eyes"; text: qsTranslate("Apertone", "Eyes"); enabled: !editor.aiBusy; Layout.fillWidth: true; onClicked: editor.beginAIMaskSelection("eyes") }
            ApertoneButton { objectName: "localAI.lips"; text: qsTranslate("Apertone", "Lips"); enabled: !editor.aiBusy; Layout.fillWidth: true; onClicked: editor.beginAIMaskSelection("lips") }
        }
        Label {
            visible: editor.aiObjectPicking
            text: editor.aiPickKind === "object" ? qsTranslate("Apertone", "Click the object in the photo.") : qsTranslate("Apertone", "Click a face in the photo.")
            wrapMode: Text.Wrap; Layout.fillWidth: true; color: Theme.accent
        }
        ProgressBar { visible: editor.aiBusy && editor.aiOperation === "AI selection"; value: editor.aiProgress; Layout.fillWidth: true }
        ApertoneButton {
            text: qsTranslate("Apertone", "Cancel")
            visible: (editor.aiBusy && editor.aiOperation === "AI selection") || editor.aiObjectPicking
            onClicked: editor.cancelAI()
            Layout.fillWidth: true
        }
        ApertoneComboBox {
            objectName: "localMaskChoice"
            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
            model: editor.localMasks
            textRole: "name"
            currentIndex: {for(let i=0;i<model.length;++i) if(model[i].id===editor.localState.id)return i;return -1}
            onActivated: {editor.selectLocalMask(model[currentIndex].id);editor.localMode=true}
        }
        Label {
            visible: !panel.selected
            text: qsTranslate("Apertone", "Add a mask to adjust part of the photo.")
            wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: 11
            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
        }
        ColumnLayout {
            visible: panel.selected
            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
            ApertoneTextField {
                objectName: "localMaskName"
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
                maximumLength: 80; text: editor.localState.name ?? ""
                onEditingFinished: if(panel.selected)editor.renameLocalMask(text)
                Accessible.name: qsTranslate("Apertone", "Mask name")
            }
            LocalCheckBox {objectName:"localMaskEnabled";text: qsTranslate("Apertone", "Enabled");checked: editor.localState.enabled ?? false;onToggled: editor.setLocalEnabled(checked)}
            LocalCheckBox {objectName:"localMaskInvert";text: qsTranslate("Apertone", "Invert mask");checked: editor.localState.inverted ?? false;onToggled: editor.setLocalInverted(checked)}
            RowLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
                ApertoneButton {objectName:"localMaskUp";text: "↑";implicitWidth:36;Layout.maximumWidth:36;Accessible.name:qsTranslate("Apertone", "Move mask up");onClicked:editor.moveLocalMask(-1)}
                ApertoneButton {objectName:"localMaskDown";text: "↓";implicitWidth:36;Layout.maximumWidth:36;Accessible.name:qsTranslate("Apertone", "Move mask down");onClicked:editor.moveLocalMask(1)}
                ApertoneButton {objectName:"localMaskDelete";text:qsTranslate("Apertone", "Delete mask");Layout.fillWidth:true;Layout.minimumWidth:0;onClicked:editor.removeLocalMask()}
            }
            ApertoneComboBox {
                objectName: "localComponentChoice"
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
                model: editor.localComponents.map(component => ({id:component.id,label:(component.operation<0?qsTranslate("Apertone", "Base selection"):panel.operationName(component.operation))+" · "+panel.kindName(component.shape)}))
                textRole: "label"
                currentIndex: (editor.localState.componentIndex ?? -1)+1
                onActivated: {editor.selectLocalComponent(model[currentIndex].id);editor.localMode=true}
                Accessible.name: qsTranslate("Apertone", "Selection component")
            }
            ColumnLayout {
                visible: (editor.localState.componentIndex ?? -1)>=0
                Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width
                ApertoneComboBox {
                    objectName:"localComponentOperation"
                    Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width
                    model:panel.operationNames;currentIndex:editor.localState.componentOperation ?? 0
                    onActivated:editor.setLocalComponentOperation(panel.operations[currentIndex])
                    Accessible.name:qsTranslate("Apertone", "Component operation")
                }
                LocalCheckBox {objectName:"localComponentEnabled";text:qsTranslate("Apertone", "Component enabled");checked:editor.localState.componentEnabled ?? true;onToggled:editor.setLocalComponentEnabled(checked)}
                LocalCheckBox {objectName:"localComponentInvert";text:qsTranslate("Apertone", "Invert component");checked:editor.localState.componentInverted ?? false;onToggled:editor.setLocalComponentInverted(checked)}
                RowLayout {
                    Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width
                    ApertoneButton {objectName:"localComponentUp";text:"↑";implicitWidth:36;Layout.maximumWidth:36;Accessible.name:qsTranslate("Apertone", "Move component up");onClicked:editor.moveLocalComponent(-1)}
                    ApertoneButton {objectName:"localComponentDown";text:"↓";implicitWidth:36;Layout.maximumWidth:36;Accessible.name:qsTranslate("Apertone", "Move component down");onClicked:editor.moveLocalComponent(1)}
                    ApertoneButton {objectName:"localRemoveComponent";text:qsTranslate("Apertone", "Remove component");Layout.fillWidth:true;Layout.minimumWidth:0;onClicked:editor.removeLocalComponent()}
                }
            }
            ApertoneButton {
                id: combine
                objectName: "localCombineToggle"
                text:qsTranslate("Apertone", "Combine selections")
                checkable:true
                Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width
            }
            ColumnLayout {
                visible:combine.checked
                Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width
                ApertoneComboBox {id:componentKind;objectName:"localComponentKind";model:panel.kindNames;currentIndex:2;Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width;Accessible.name:qsTranslate("Apertone", "Component type")}
                ApertoneComboBox {id:componentOperation;objectName:"localNewOperation";model:panel.operationNames;Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width;Accessible.name:qsTranslate("Apertone", "Component operation")}
                ApertoneButton {objectName:"localAddComponent";text:qsTranslate("Apertone", "Add component");Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width;onClicked:editor.addLocalComponent(panel.kinds[componentKind.currentIndex],panel.operations[componentOperation.currentIndex])}
            }
            ApertoneButton {
                objectName:"localEditOnPhoto"
                text:qsTranslate("Apertone", "Edit mask on photo")
                checkable:true;checked:editor.localMode
                Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width
                onClicked:editor.localMode=!editor.localMode
            }
            ColumnLayout {
                visible:editor.localState.shape==="brush"
                Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width
                Label {Layout.fillWidth:true;Layout.minimumWidth:0;wrapMode:Text.Wrap;text:qsTranslate("Apertone", "Brush size")+" · "+Number(editor.localBrushSize).toFixed(1)+"%";font.pixelSize:11}
                Slider {objectName:"localBrushSize";from:.1;to:100;value:editor.localBrushSize;onMoved:editor.localBrushSize=value;Layout.fillWidth:true;Accessible.name:qsTranslate("Apertone", "Brush size")}
                Label {Layout.fillWidth:true;Layout.minimumWidth:0;wrapMode:Text.Wrap;text:qsTranslate("Apertone", "Brush opacity")+" · "+Number(editor.localBrushOpacity).toFixed(0)+"%";font.pixelSize:11}
                Slider {objectName:"localBrushOpacity";from:0;to:100;value:editor.localBrushOpacity;onMoved:editor.localBrushOpacity=value;Layout.fillWidth:true;Accessible.name:qsTranslate("Apertone", "Brush opacity")}
                LocalCheckBox {objectName:"localBrushErase";text:qsTranslate("Apertone", "Erase");checked:editor.localBrushErase;onToggled:editor.localBrushErase=checked}
                ApertoneButton {objectName:"localClearBrush";text:qsTranslate("Apertone", "Clear brush strokes");enabled:(editor.localState.strokeCount ?? 0)>0;Layout.fillWidth:true;Layout.minimumWidth:0;onClicked:editor.clearLocalBrush()}
            }
            ColumnLayout {
                visible:editor.localState.shape==="luminance"||editor.localState.shape==="color"
                Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width
                Label {
                    text:editor.localSampling?qsTranslate("Apertone", "Sampling…"):qsTranslate("Apertone", "Click the photo to sample the range.")
                    wrapMode:Text.Wrap;font.pixelSize:11;color:Theme.accent
                    Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width
                }
                Rectangle {visible:editor.localState.shape==="color";color:editor.localState.referenceColor ?? "#808080";implicitHeight:22;Layout.fillWidth:true;Accessible.name:qsTranslate("Apertone", "Sampled color")}
                LocalSlider {visible:editor.localState.shape==="luminance";parameter:"rangeLow";label:qsTranslate("Apertone", "Luminance minimum");minimum:0;maximum:editor.localState.rangeHigh ?? 100;step:.1}
                LocalSlider {visible:editor.localState.shape==="luminance";parameter:"rangeHigh";label:qsTranslate("Apertone", "Luminance maximum");minimum:editor.localState.rangeLow ?? 0;maximum:100;step:.1}
                LocalSlider {visible:editor.localState.shape==="color";parameter:"colorTolerance";label:qsTranslate("Apertone", "Color tolerance");minimum:0;maximum:100;step:.1}
            }
            LocalSlider {visible:editor.localState.shape!=="bitmap";parameter:"feather";label:qsTranslate("Apertone", "Feather");minimum:0;maximum:100}
            LocalSlider {visible:editor.localState.shape==="bitmap";parameter:"edgeShift";label:qsTranslate("Apertone", "Edge shift");minimum:-100;maximum:100}
            LocalSlider {visible:editor.localState.shape==="bitmap";parameter:"edgeFeather";label:qsTranslate("Apertone", "Edge feather");minimum:0;maximum:100}
            LocalSlider {visible:editor.localState.shape==="radial";parameter:"angle";label:qsTranslate("Apertone", "Rotation");minimum:-180;maximum:180}
            LocalSlider {parameter:"opacity";label:qsTranslate("Apertone", "Mask opacity");minimum:0;maximum:100}
            LocalSlider {parameter:"blur";label:qsTranslate("Apertone", "Blur");minimum:0;maximum:100}
            LocalCheckBox {objectName:"localOverlayToggle";text:qsTranslate("Apertone", "Show mask overlay");checked:editor.localOverlay;onToggled:editor.localOverlay=checked}
            RowLayout {
                Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width
                Label {text:qsTranslate("Apertone", "Overlay color");Layout.fillWidth:true;Layout.minimumWidth:0;font.pixelSize:11}
                Repeater {
                    model:["#ff2666","#00cbea","#67e26a"]
                    ApertoneButton {
                        required property string modelData
                        objectName:"localOverlayColor."+modelData
                        implicitWidth:28;implicitHeight:28;Layout.preferredWidth:28;Layout.maximumWidth:28
                        Accessible.name:qsTranslate("Apertone", "Overlay color")+" "+modelData
                        onClicked:editor.localOverlayColor=modelData
                        background:Rectangle {color:parent.modelData;radius:4;border.width:editor.localOverlayColor===parent.modelData?3:0;border.color:"white"}
                    }
                }
            }
            Label {text:qsTranslate("Apertone", "Overlay opacity");font.pixelSize:11}
            Slider {objectName:"localOverlayOpacity";from:0;to:1;value:editor.localOverlayOpacity;onMoved:editor.localOverlayOpacity=value;Layout.fillWidth:true;Layout.minimumWidth:0;Accessible.name:qsTranslate("Apertone", "Overlay opacity")}
            LocalSlider {parameter:"exposure";label:qsTranslate("Apertone", "Exposure");minimum:-5;maximum:5;step:.01}
            LocalSlider {parameter:"contrast";label:qsTranslate("Apertone", "Contrast")}
            LocalSlider {parameter:"highlights";label:qsTranslate("Apertone", "Highlights")}
            LocalSlider {parameter:"shadows";label:qsTranslate("Apertone", "Shadows")}
            LocalSlider {parameter:"whites";label:qsTranslate("Apertone", "Whites")}
            LocalSlider {parameter:"blacks";label:qsTranslate("Apertone", "Blacks")}
            LocalSlider {parameter:"vibrance";label:qsTranslate("Apertone", "Vibrance")}
            LocalSlider {parameter:"saturation";label:qsTranslate("Apertone", "Saturation")}
            Label {
                visible:editor.localMode
                text:editor.localState.shape==="brush"?qsTranslate("Apertone", "Paint on the photo; each stroke is one Undo."):qsTranslate("Apertone", "Drag empty photo to redraw; drag handles to move or resize.")
                wrapMode:Text.Wrap;font.pixelSize:11;color:Theme.accent
                Layout.fillWidth:true;Layout.minimumWidth:0;Layout.maximumWidth:panel.width
            }
        }
    }
}
