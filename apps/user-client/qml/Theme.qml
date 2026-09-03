pragma Singleton
import QtQuick

// 设计令牌:配色取自 ui-ux-pro-max 技能库 Sustainable Energy 方案
// (#059669 生态绿),底色与文字用中性灰绿而非蓝灰,整体去"AI 仪表盘"感
QtObject {
    readonly property string fontFamily: Qt.application.font.family
    readonly property color primary: "#059669"
    readonly property color primaryDark: "#047857"
    readonly property color primarySoft: "#E7F5EE"
    readonly property color background: "#F6F7F6"
    readonly property color surface: "#FFFFFF"
    readonly property color text: "#1C2B24"
    readonly property color textMuted: "#66756D"
    readonly property color border: "#E4E9E6"
    readonly property color danger: "#DC2626"
    readonly property color warning: "#D97706"
    readonly property color success: "#059669"
    readonly property int radiusSmall: 8
    readonly property int radius: 14
    readonly property int radiusLarge: 20
}
