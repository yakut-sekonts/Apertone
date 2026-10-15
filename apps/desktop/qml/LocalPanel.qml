import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: panel
    objectName: "localPanel"
    Layout.preferredWidth: 250
    Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: parent.width
    property bool selected: !!editor.localState.id
    Button {
        objectName: "localPanelToggle"
        text: qsTranslate("Luma", "Local adjustments")
        checkable: true; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
        id: expanded
    }
    ColumnLayout {
        visible: expanded.checked
        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
        enabled: editor.hasPhoto && !editor.busy && !editor.editingLocked && !editor.original
        ColumnLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
            Button { objectName: "addLinearMask"; text: qsTranslate("Luma", "Linear gradient"); Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width; onClicked: editor.addLocalMask("linear") }
            Button { objectName: "addRadialMask"; text: qsTranslate("Luma", "Radial gradient"); Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width; onClicked: editor.addLocalMask("radial") }
        }
        ComboBox {
            objectName: "localMaskChoice"
            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width; model: editor.localMasks; textRole: "name"
            currentIndex: { for (let i=0; i<model.length; ++i) if(model[i].id===editor.localState.id) return i; return -1 }
            onActivated: { editor.selectLocalMask(model[currentIndex].id); editor.localMode = true }
        }
        Label { visible: !panel.selected; text: qsTranslate("Luma", "Add a mask to adjust part of the photo."); wrapMode: Text.Wrap; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width; color: "#858c97"; font.pixelSize: 11 }
        ColumnLayout {
            visible: panel.selected; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
            TextField { objectName: "localMaskName"; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width; maximumLength: 80; text: editor.localState.name ?? ""; onEditingFinished: if(panel.selected) editor.renameLocalMask(text); Accessible.name: qsTranslate("Luma", "Mask name") }
            ColumnLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
                CheckBox { text: qsTranslate("Luma", "Enabled"); checked: editor.localState.enabled ?? false; onToggled: editor.setLocalEnabled(checked); Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width }
                CheckBox { text: qsTranslate("Luma", "Invert"); checked: editor.localState.inverted ?? false; onToggled: editor.setLocalInverted(checked) }
            }
            RowLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
                Button { text: "↑"; implicitWidth: 36; Layout.preferredWidth: 36; Layout.maximumWidth: 36; Accessible.name: qsTranslate("Luma", "Move mask up"); onClicked: editor.moveLocalMask(-1) }
                Button { text: "↓"; implicitWidth: 36; Layout.preferredWidth: 36; Layout.maximumWidth: 36; Accessible.name: qsTranslate("Luma", "Move mask down"); onClicked: editor.moveLocalMask(1) }
                Button { text: qsTranslate("Luma", "Delete mask"); onClicked: editor.removeLocalMask(); Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width }
            }
            Button { objectName: "localEditOnPhoto"; text: qsTranslate("Luma", "Edit mask on photo"); checkable: true; checked: editor.localMode; onClicked: editor.localMode = !editor.localMode; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width }
            CheckBox { objectName: "localOverlayToggle"; text: qsTranslate("Luma", "Show mask overlay"); checked: editor.localOverlay; onToggled: editor.localOverlay = checked }
            RowLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width
                Label { text: qsTranslate("Luma", "Overlay color"); Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width; font.pixelSize: 11 }
                Repeater {
                    model: ["#ff2666", "#00cbea", "#67e26a"]
                    Button { required property string modelData; Layout.preferredWidth: 28; Layout.maximumWidth: 28; implicitWidth: 28; implicitHeight: 28; Accessible.name: qsTranslate("Luma", "Overlay color") + " " + modelData; onClicked: editor.localOverlayColor = modelData
                        background: Rectangle { color: parent.modelData; radius: 4; border.width: editor.localOverlayColor===parent.modelData ? 3 : 0; border.color: "white" }
                    }
                }
            }
            Label { text: qsTranslate("Luma", "Overlay opacity"); font.pixelSize: 11 }
            Slider { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width; from: 0; to: 1; value: editor.localOverlayOpacity; onMoved: editor.localOverlayOpacity = value; Accessible.name: qsTranslate("Luma", "Overlay opacity") }
            LocalSlider { parameter: "opacity"; label: qsTranslate("Luma", "Mask opacity"); minimum: 0; maximum: 100 }
            LocalSlider { parameter: "feather"; label: qsTranslate("Luma", "Feather"); minimum: 0; maximum: 100 }
            LocalSlider { visible: editor.localState.shape==="radial"; parameter: "angle"; label: qsTranslate("Luma", "Rotation"); minimum: -180; maximum: 180 }
            LocalSlider { parameter: "exposure"; label: qsTranslate("Luma", "Exposure"); minimum: -5; maximum: 5; step: 0.01 }
            LocalSlider { parameter: "contrast"; label: qsTranslate("Luma", "Contrast") }
            LocalSlider { parameter: "highlights"; label: qsTranslate("Luma", "Highlights") }
            LocalSlider { parameter: "shadows"; label: qsTranslate("Luma", "Shadows") }
            LocalSlider { parameter: "whites"; label: qsTranslate("Luma", "Whites") }
            LocalSlider { parameter: "blacks"; label: qsTranslate("Luma", "Blacks") }
            LocalSlider { parameter: "vibrance"; label: qsTranslate("Luma", "Vibrance") }
            LocalSlider { parameter: "saturation"; label: qsTranslate("Luma", "Saturation") }
            Label { visible: editor.localMode; text: qsTranslate("Luma", "Drag empty photo to redraw; drag handles to move or resize."); wrapMode: Text.Wrap; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: panel.width; font.pixelSize: 11; color: "#dfae77" }
        }
    }
}
