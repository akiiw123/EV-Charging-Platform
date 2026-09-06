pragma Singleton
import QtQuick

// 设计令牌:与管理端统一为参考图(ChargePilot)提取的"信号蓝 + 藏青墨 + 浅灰底"
// 品牌家族;绿色仅作成功/可用状态色,不再作主色
QtObject {
    property string currentTheme: "default"
    readonly property var colors: ({
        "default": {primary:"#1B6EF3", dark:"#155FD6", soft:"#EAF2FF", bg:"#F4F6F8", text:"#0B1531"},
        "porcelain": {primary:"#497596", dark:"#34546F", soft:"#EAF0F4", bg:"#FBFAF6", text:"#243847"},
        "emerald": {primary:"#0F9279", dark:"#096E5B", soft:"#E1F4ED", bg:"#F1F7F4", text:"#17392F"}
    })
    readonly property var palette: colors[currentTheme] || colors.default
    readonly property string fontFamily: Qt.application.font.family
    readonly property color primary: palette.primary
    readonly property color primaryDark: palette.dark
    readonly property color primarySoft: palette.soft
    readonly property color background: palette.bg
    readonly property color surface: "#FFFFFF"
    readonly property color text: palette.text
    readonly property color textMuted: "#6B7480"
    readonly property color border: "#E3E7EC"
    readonly property color danger: "#DC2626"
    readonly property color warning: "#D97706"
    readonly property color success: "#16A34A"
    readonly property int radiusSmall: 8
    readonly property int radius: 14
    readonly property int radiusLarge: 20
}
