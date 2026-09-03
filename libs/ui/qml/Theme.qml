pragma Singleton
import QtQuick

QtObject {
    id: theme
    property string currentTheme: "default"
    property bool animationsEnabled: true
    property real fontScale: 1.0

    readonly property var palettes: ({
        "default":   { bg:"#F6F7F6", bg2:"#EDF0EE", surface:"#FFFFFF", elevated:"#FFFFFF", hover:"#E7F5EE", selected:"#D6EFE3", border:"#DCE3DF", strong:"#A8B5AE", text:"#1C2B24", secondary:"#4E5F56", muted:"#66756D", accent:"#059669", hoverAccent:"#04875F", pressed:"#047857", shadow:"#24141C18", overlay:"#66071A12", focus:"#5ED9A9" },
        "midnight":  { bg:"#07111F", bg2:"#0B1728", surface:"#102039", elevated:"#162B47", hover:"#193453", selected:"#123F55", border:"#29415D", strong:"#466887", text:"#EAF4FF", secondary:"#B3C5D8", muted:"#8299B2", accent:"#18C8F4", hoverAccent:"#46D6F7", pressed:"#09A7D2", shadow:"#73000000", overlay:"#A0000710", focus:"#75E3FF" },
        "aurora":    { bg:"#0B1020", bg2:"#11172B", surface:"#171E35", elevated:"#202944", hover:"#282F50", selected:"#30265E", border:"#333D5F", strong:"#59658C", text:"#F2F0FF", secondary:"#C0BDE1", muted:"#8E91B3", accent:"#8B75FF", hoverAccent:"#A995FF", pressed:"#6F55E9", shadow:"#78000000", overlay:"#A0060914", focus:"#B6A9FF" },
        "graphite":  { bg:"#121416", bg2:"#191C20", surface:"#202429", elevated:"#292E34", hover:"#31363C", selected:"#3B3021", border:"#3B4148", strong:"#606872", text:"#F5F2EC", secondary:"#C9C1B6", muted:"#918B83", accent:"#F2A93B", hoverAccent:"#FFC15C", pressed:"#D58B20", shadow:"#80000000", overlay:"#A00A0B0C", focus:"#FFD083" },
        "emerald":   { bg:"#071713", bg2:"#0C201A", surface:"#112B23", elevated:"#17382E", hover:"#1D4438", selected:"#15503E", border:"#285044", strong:"#447365", text:"#E9FFF7", secondary:"#B4D6CA", muted:"#7F9E93", accent:"#2ED39A", hoverAccent:"#55E1AE", pressed:"#1AAF7D", shadow:"#78000000", overlay:"#A0030E0B", focus:"#7AEBBF" },
        "porcelain": { bg:"#F5F8FC", bg2:"#EDF2F8", surface:"#FFFFFF", elevated:"#FFFFFF", hover:"#EEF5FF", selected:"#E2EDFF", border:"#D7E0EB", strong:"#A8B8CB", text:"#17263A", secondary:"#50647D", muted:"#75869A", accent:"#3478F6", hoverAccent:"#4A8AFA", pressed:"#2462D4", shadow:"#240E2238", overlay:"#660B1728", focus:"#72A4FF" },
        "contrast":  { bg:"#000000", bg2:"#090909", surface:"#111111", elevated:"#1A1A1A", hover:"#282828", selected:"#003D4A", border:"#777777", strong:"#FFFFFF", text:"#FFFFFF", secondary:"#F0F0F0", muted:"#CCCCCC", accent:"#00E5FF", hoverAccent:"#65F2FF", pressed:"#00B8D0", shadow:"#CC000000", overlay:"#DD000000", focus:"#FFFF00" }
    })
    readonly property var p: palettes[currentTheme] || palettes.midnight

    readonly property color backgroundPrimary: p.bg
    readonly property color backgroundSecondary: p.bg2
    readonly property color surface: p.surface
    readonly property color surfaceElevated: p.elevated
    readonly property color surfaceHover: p.hover
    readonly property color surfaceSelected: p.selected
    readonly property color borderSubtle: p.border
    readonly property color borderStrong: p.strong
    readonly property color textPrimary: p.text
    readonly property color textSecondary: p.secondary
    readonly property color textMuted: p.muted
    readonly property color accent: p.accent
    readonly property color accentHover: p.hoverAccent
    readonly property color accentPressed: p.pressed
    readonly property color success: "#10B981"
    readonly property color warning: "#D97706"
    readonly property color danger: "#DC2626"
    readonly property color info: "#3B82F6"
    readonly property var chartPalette: [accent, success, warning, "#9B87F5", danger, info]
    readonly property color shadowColor: p.shadow
    readonly property color overlayColor: p.overlay
    readonly property color focusRing: p.focus
    // 用户端第一版设计令牌的兼容别名，迁移时不需要改动业务页面。
    readonly property color primary: accent
    readonly property color primaryDark: accentPressed
    readonly property color primarySoft: surfaceSelected
    readonly property color background: backgroundPrimary
    readonly property color text: textPrimary
    readonly property color border: borderSubtle

    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 20
    readonly property int space6: 24
    readonly property int radiusSmall: 8
    readonly property int radiusMedium: 12
    readonly property int radiusLarge: 16
    readonly property int radius: radiusLarge
    readonly property int controlHeight: 40
    readonly property int rowHeight: 46
    readonly property int fontCaption: Math.round(11 * fontScale)
    readonly property int fontBody: Math.round(13 * fontScale)
    readonly property int fontSubtitle: Math.round(15 * fontScale)
    readonly property int fontTitle: Math.round(20 * fontScale)
    readonly property int fontDisplay: Math.round(28 * fontScale)
    readonly property int durationFast: animationsEnabled ? 140 : 0
    readonly property int durationNormal: animationsEnabled ? 200 : 0
}
