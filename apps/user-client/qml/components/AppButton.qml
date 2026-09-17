/*
 * 文件职责：通用按钮：统一主次按钮、禁用、悬停、按下和焦点样式。
 * 对接关系：属于 UserAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import QtQuick.Controls
import ChargingUser

Button {
    id: control
    property string variant: "primary"
    implicitHeight: 44
    font.pixelSize: 14
    font.bold: true
    leftPadding: 18
    rightPadding: 18
    contentItem: Text {
        elide: Text.ElideRight
        text: control.text
        font: control.font
        color: control.variant === "primary" ? Theme.text
              : control.variant === "danger" ? Theme.danger : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        opacity: control.enabled ? 1 : 0.5
    }
    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.variant === "primary"
               ? (control.down ? Theme.primaryDark : Theme.primary)
               : control.hovered ? Theme.primarySoft : Theme.surface
        border.width: control.variant === "primary" ? 0 : 1
        border.color: control.variant === "danger" ? Theme.danger : Theme.border
        opacity: control.enabled ? 1 : 0.55
        Behavior on color { ColorAnimation { duration: 140 } }
    }
}
