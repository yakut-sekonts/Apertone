import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    objectName: "library.photoMap"
    title: qsTranslate("Apertone", "Photo locations")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(1050, parent.width - 40)
    height: Math.min(700, parent.height - 40)
    modal: true
    standardButtons: Dialog.Close
    property bool loading: true
    property var points: []
    property int current: -1
    property string error: ""
    onOpened: {
        loading = true; points = []; current = -1; error = ""; zoom.value = 1
        library.loadPhotoLocations()
    }
    Connections {
        target: library
        function onPhotoLocationsReady(locations, failure) {
            if (!dialog.visible) return
            dialog.points = locations
            dialog.error = failure
            dialog.loading = false
        }
    }
    contentItem: ColumnLayout {
        RowLayout {
            Label { text: qsTranslate("Apertone", "Map zoom"); color: Theme.muted }
            ApertoneSlider { id: zoom; from: 1; to: 6; value: 1; Layout.fillWidth: true; Accessible.name: qsTranslate("Apertone", "Map zoom") }
            ApertoneButton { text: qsTranslate("Apertone", "Fit"); onClicked: { zoom.value = 1; mapView.contentX = 0; mapView.contentY = 0 } }
        }
        Flickable {
            id: mapView
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: Math.max(width, surface.width)
            contentHeight: Math.max(height, surface.height)
            Item {
                id: surface
                width: Math.min(mapView.width, mapView.height * 2) * zoom.value
                height: width / 2
                x: Math.max(0, (mapView.width - width) / 2)
                y: Math.max(0, (mapView.height - height) / 2)
                Image { anchors.fill: parent; source: "qrc:/map/world-land.png"; smooth: true }
                Repeater {
                    model: dialog.points
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        x: (modelData.longitude + 180) * surface.width / 360 - width / 2
                        y: (90 - modelData.latitude) * surface.height / 180 - height / 2
                        width: 16; height: 16; radius: 8
                        z: dialog.current === index ? 1 : 0
                        color: dialog.current === index ? Theme.accent : "#ffe0a0"
                        border.color: "#342719"; border.width: 2
                        MouseArea { anchors.fill: parent; hoverEnabled: true; onClicked: dialog.current = parent.index }
                        ToolTip.visible: dialog.current === index
                        ToolTip.text: modelData.name
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            BusyIndicator { running: dialog.loading; visible: running; implicitWidth: 24; implicitHeight: 24 }
            Label {
                Layout.fillWidth: true; textFormat: Text.PlainText; elide: Text.ElideMiddle
                text: dialog.error || (dialog.current >= 0
                      ? dialog.points[dialog.current].name + " · " + dialog.points[dialog.current].latitude.toFixed(5) + ", " + dialog.points[dialog.current].longitude.toFixed(5)
                      : dialog.points.length > 0 ? qsTranslate("Apertone", "Select a pin to open the photo")
                      : dialog.loading ? qsTranslate("Apertone", "Reading photo locations…")
                                       : qsTranslate("Apertone", "The selection has no GPS coordinates"))
            }
            ApertoneButton {
                text: qsTranslate("Apertone", "Open photo")
                enabled: dialog.current >= 0
                onClicked: { library.openMapPhoto(dialog.points[dialog.current].id); dialog.close() }
            }
        }
    }
}
