pragma Singleton
import QtQuick

QtObject {
    // Standalone component tests and reusable dialogs do not always provide an
    // appearance controller. Application roots bind this fallback to the
    // persisted controller when they own one.
    property bool dark: true

    readonly property color window: dark ? "#292929" : "#dedede"
    readonly property color panel: dark ? "#252525" : "#f3f3f3"
    readonly property color panelRaised: dark ? "#323232" : "#e8e8e8"
    readonly property color control: dark ? "#393939" : "#e2e2e2"
    readonly property color controlHover: dark ? "#444444" : "#d6d6d6"
    readonly property color divider: dark ? "#484848" : "#c4c4c4"
    readonly property color dividerSoft: dark ? "#303030" : "#dcdcdc"
    readonly property color thumbnailMat: dark ? "#3c3c3c" : "#d2d2d2"
    readonly property color selectedMat: dark ? "#525252" : "#eeeeee"
    readonly property color text: dark ? "#f7f7f8" : "#15201f"
    readonly property color muted: dark ? "#a0a1a8" : "#5f6968"
    readonly property color faint: dark ? "#8a8c92" : "#6f7978"
    readonly property color accent: dark ? "#4a8ad6" : "#176f72"
    readonly property color accentSoft: dark ? "#1c2b3d" : "#dcebea"
    readonly property color positive: dark ? "#5fc39c" : "#277853"
    readonly property color negative: dark ? "#e56a72" : "#b93636"
    readonly property color warning: dark ? "#d9a441" : "#a86f0c"
    readonly property color cropHandle: "#dfae77"
    readonly property color errorSurface: dark ? "#2c1a20" : "#ffedea"
    readonly property color canvas: "#393939"
    readonly property color canvasRaised: "#2d2d2d"
    readonly property color canvasText: "#d6d9de"
    readonly property color canvasMuted: "#8f959e"

    readonly property int toolbarHeight: 44
    readonly property int footerHeight: 32
    readonly property int controlHeight: 26
    readonly property int compactControlHeight: 22
    readonly property int sliderHeight: 18
    readonly property int sliderHandleSize: 10
    readonly property int sliderTrackHeight: 2
    readonly property int checkHeight: 18
    readonly property int checkIndicatorSize: 12
    readonly property int panelSpacing: 6
    readonly property int radius: dark ? 3 : 4
    readonly property int panelPadding: 9
    readonly property int sidebarWidth: dark ? 196 : 210
    readonly property int inspectorWidth: dark ? 276 : 286
    readonly property int filmstripHeight: 96
}
