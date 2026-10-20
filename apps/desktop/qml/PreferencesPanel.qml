import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: panel
    objectName: "preferencesPanel"
    title: qsTranslate("Luma", "Settings")
    modal: true
    anchors.centerIn: parent
    width: Math.min(parent.width - 60, 440)
    standardButtons: Dialog.Close
    ColumnLayout {
        width: parent.width
        spacing: 14
        Label { objectName: "settingsVersion"; text: "Luma " + Qt.application.version; font.pixelSize: 20; font.weight: Font.DemiBold; Layout.fillWidth: true }
        Label { objectName: "settingsLanguageLabel"; text: qsTranslate("Luma", "Interface language"); Layout.fillWidth: true; wrapMode: Text.Wrap }
        ComboBox {
            id: languageChoice
            objectName: "languageChoice"
            Layout.fillWidth: true
            model: localization.languages
            textRole: "name"
            valueRole: "code"
            currentIndex: {
                for (let i = 0; i < count; ++i)
                    if (model[i].code === localization.language) return i
                return 0
            }
            onActivated: localization.selectLanguage(model[currentIndex].code)
            Accessible.name: qsTranslate("Luma", "Interface language")
        }
        Label { objectName: "settingsEffect"; text: qsTranslate("Luma", "Changes take effect immediately."); Layout.fillWidth: true; wrapMode: Text.Wrap }
        Label { text: qsTranslate("Luma", "Preview cache (MiB)"); Layout.fillWidth: true }
        SpinBox {
            objectName: "settings.previewCache"
            Layout.fillWidth: true
            from: 64; to: 2048; stepSize: 64
            value: editor.previewCacheMiB
            editable: true
            onValueModified: editor.previewCacheMiB = value
            Accessible.name: qsTranslate("Luma", "Preview cache (MiB)")
        }
        Button {
            text: qsTranslate("Luma", "Clear preview cache")
            onClicked: editor.clearPreviewCache()
        }
        CheckBox {
            text: qsTranslate("Luma", "Use Metal for photo previews")
            checked: editor.metalPreview
            onToggled: editor.metalPreview = checked
        }
        Label { text: qsTranslate("Luma", localization.error); textFormat: Text.PlainText; visible: text.length > 0; color: "#ffc1bc"; Layout.fillWidth: true; wrapMode: Text.Wrap }
    }
}
