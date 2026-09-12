pragma Singleton
import QtQuick
import Charging.UI as UI

// 设计令牌:色板/状态色/圆角全部委托给共享设计系统 Charging.UI,
// 用户端只保留自己的令牌名映射,不再维护第二套颜色值,保证与管理端视觉一致。
// 主题名对应共享色板:信号蓝=default、云白蓝=porcelain、翡翠绿=emerald。
QtObject {
    property string currentTheme: "default"
    readonly property var palette: {
        var shared = {}
        shared["default"] = UI.Theme.palettes.default
        shared["porcelain"] = UI.Theme.palettes.porcelain
        shared["emerald"] = UI.Theme.palettes.emerald
        return shared[currentTheme] || shared["default"]
    }
    readonly property string fontFamily: Qt.application.font.family
    readonly property color primary: palette.accent
    readonly property color primaryDark: palette.pressed
    readonly property color primarySoft: palette.hover
    readonly property color primarySelected: palette.selected
    readonly property color background: palette.bg
    readonly property color backgroundSecondary: palette.bg2
    readonly property color surface: palette.surface
    readonly property color text: palette.text
    readonly property color textMuted: palette.muted
    readonly property color border: palette.border
    readonly property color danger: UI.Theme.danger
    readonly property color warning: UI.Theme.warning
    readonly property color success: UI.Theme.success
    readonly property int radiusSmall: 6
    readonly property int radius: 8
    readonly property int radiusLarge: 10
}
