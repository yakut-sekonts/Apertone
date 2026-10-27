pragma Singleton
import QtQuick

QtObject {
    readonly property bool dark: appearance.dark

    readonly property color window: dark ? "#171719" : "#e8eeed"
    readonly property color panel: dark ? "#131217" : "#ffffff"
    readonly property color panelRaised: dark ? "#212023" : "#f2f5f4"
    readonly property color control: dark ? "#2a292d" : "#e4e9e8"
    readonly property color controlHover: dark ? "#343339" : "#dce3e2"
    readonly property color divider: dark ? "#323136" : "#d9dfde"
    readonly property color dividerSoft: dark ? "#252428" : "#edf0ef"
    readonly property color text: dark ? "#f7f7f8" : "#15201f"
    readonly property color muted: dark ? "#a0a1a8" : "#5f6968"
    readonly property color faint: dark ? "#8a8c92" : "#6f7978"
    readonly property color accent: dark ? "#4a8ad6" : "#176f72"
    readonly property color accentSoft: dark ? "#1c2b3d" : "#dcebea"
    readonly property color positive: dark ? "#5fc39c" : "#277853"
    readonly property color negative: dark ? "#e56a72" : "#b93636"
    readonly property color warning: dark ? "#d9a441" : "#a86f0c"
    readonly property color errorSurface: dark ? "#2c1a20" : "#ffedea"
    readonly property color canvas: "#0b0c0e"
    readonly property color canvasRaised: "#101114"
    readonly property color canvasText: "#d6d9de"
    readonly property color canvasMuted: "#8f959e"

    readonly property int toolbarHeight: dark ? 43 : 49
    readonly property int footerHeight: 24
    readonly property int controlHeight: 30
    readonly property int compactControlHeight: 26
    readonly property int radius: dark ? 3 : 7
    readonly property int panelPadding: 12
    readonly property int sidebarWidth: dark ? 196 : 210
    readonly property int inspectorWidth: dark ? 276 : 286
    readonly property int filmstripHeight: 108
}
