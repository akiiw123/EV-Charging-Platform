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
