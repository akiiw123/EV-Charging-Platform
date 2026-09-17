/*
 * 文件职责：通用按钮：统一主次按钮、禁用、悬停、按下和焦点样式。
 * 对接关系：属于 Charging.UI 共享设计系统；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick

FocusScope {
    id: control
    property string text: ""
    property string variant: "primary"
    property bool loading: false
    signal clicked()
    implicitHeight: Theme.controlHeight
    implicitWidth: Math.max(92, label.implicitWidth + 32)
    opacity: enabled ? 1 : .48
    activeFocusOnTab: true
    Keys.onSpacePressed: if (enabled && !loading) clicked()
    Keys.onReturnPressed: if (enabled && !loading) clicked()
    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: control.variant === "primary" ? (mouse.pressed ? Theme.accentPressed : mouse.containsMouse ? Theme.accentHover : Theme.accent)
             : mouse.pressed ? Theme.surfaceSelected : mouse.containsMouse ? Theme.surfaceHover : "transparent"
        border.width: control.variant === "primary" ? 0 : control.activeFocus ? 2 : 1
        border.color: control.variant === "danger" ? Theme.danger : control.activeFocus ? Theme.focusRing : Theme.borderStrong
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    }
    Text {
        id: label; anchors.centerIn: parent
        text: control.loading ? "处理中…" : control.text
        color: control.variant === "primary" ? "white" : control.variant === "danger" ? Theme.danger : Theme.textPrimary
        font.pixelSize: Theme.fontBody; font.weight: Font.DemiBold
    }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor; onClicked: if (control.enabled && !control.loading) control.clicked() }
}
