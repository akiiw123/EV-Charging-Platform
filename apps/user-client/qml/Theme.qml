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
        // 主操作色:云白蓝=浅蓝 #B9D9EB、浅白黄=米黄 #F2E9DB(底色均不变);
        // pressed 为对应深色调,兼作浅色主操作上的文字/图标对比色
        table["default"] = { bg: UI.Theme.palettes.default.bg, bg2: UI.Theme.palettes.default.bg2,
                             surface: UI.Theme.palettes.default.surface,
                             hover: "#E8F0F6", selected: "#D9E6EF",
                             border: UI.Theme.palettes.default.border,
                             text: UI.Theme.palettes.default.text, secondary: UI.Theme.palettes.default.secondary,
                             muted: UI.Theme.palettes.default.muted,
                             accent: "#5E93B4", hoverAccent: "#6FA3C4", pressed: "#46748F" }
        table["gold"] = { bg: "#F8F7EF", bg2: "#F0EFE2", surface: UI.Theme.palettes.default.surface,
                          hover: "#F5EFE4", selected: "#EDE3D0", border: "#E0D6C2",
                          text: UI.Theme.palettes.default.text, secondary: UI.Theme.palettes.default.secondary,
                          muted: UI.Theme.palettes.default.muted,
                          accent: "#9C8A5E", hoverAccent: "#AD9B70", pressed: "#7E6E48" }
        table["porcelain"] = UI.Theme.palettes.porcelain
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
