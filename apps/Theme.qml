pragma Singleton
import QtQuick

// 设计令牌:与管理端统一为参考图(ChargePilot)提取的"信号蓝 + 藏青墨 + 浅灰底"
// 品牌家族;绿色仅作成功/可用状态色,不再作主色
QtObject {
    readonly property string fontFamily: Qt.application.font.family
    readonly property color primary: "#1B6EF3"
    readonly property color primaryDark: "#155FD6"
    readonly property color primarySoft: "#EAF2FF"
    readonly property color background: "#F4F6F8"
    readonly property color surface: "#FFFFFF"
    readonly property color text: "#0B1531"
    readonly property color textMuted: "#6B7480"
    readonly property color border: "#E3E7EC"
    readonly property color danger: "#DC2626"
    readonly property color warning: "#D97706"
    readonly property color success: "#16A34A"
    readonly property int radiusSmall: 8
    readonly property int radius: 14
    readonly property int radiusLarge: 20
}
