pragma Singleton
import QtQuick
import Charging.UI as UI

// 设计令牌:色板/状态色/圆角全部委托给共享设计系统 Charging.UI,
// 用户端只保留自己的令牌名映射,不再维护第二套颜色值,保证与管理端视觉一致。
// 主题名对应共享色板:护眼黄=default、云白蓝=porcelain、翡翠绿=emerald。
QtObject {
    property string currentTheme: "default"
    readonly property var palette: {
        var table = {}
        // 云白蓝:浅蓝底 #B9D9EB;浅白黄:米黄底 #F2E9DB(用户指定参考色);翡翠绿沿用共享色板
        table["default"] = { bg: "#B9D9EB", bg2: "#A8C9DC", surface: UI.Theme.palettes.default.surface,
                             hover: "#C9DFEC", selected: "#B7D4E4", border: "#9EC2D6",
                             text: UI.Theme.palettes.default.text, secondary: UI.Theme.palettes.default.secondary,
                             muted: "#5E7889",
                             accent: UI.Theme.palettes.default.accent, hoverAccent: UI.Theme.palettes.default.hoverAccent,
                             pressed: UI.Theme.palettes.default.pressed }
        table["gold"] = { bg: "#F2E9DB", bg2: "#E8DFCE", surface: UI.Theme.palettes.default.surface,
                          hover: "#F6F0E4", selected: "#EDE3D0", border: "#DDD2BC",
                          text: UI.Theme.palettes.default.text, secondary: UI.Theme.palettes.default.secondary,
                          muted: UI.Theme.palettes.default.muted,
                          accent: "#E6C34A", hoverAccent: "#EDCF66", pressed: "#A8841C" }
        table["emerald"] = UI.Theme.palettes.emerald
        return table[currentTheme] || table["default"]
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
    readonly property int radiusSmall: 4
    readonly property int radius: 6
    readonly property int radiusLarge: 8
}
