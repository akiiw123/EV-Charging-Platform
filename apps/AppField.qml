import QtQuick
import QtQuick.Controls
import ChargingUser

TextField {
    id: field
    implicitHeight: 46
    leftPadding: 14
    rightPadding: 14
    color: Theme.text
    placeholderTextColor: Theme.textMuted
    font.pixelSize: 14
    selectByMouse: true
    background: Rectangle {
        radius: 12
        color: Theme.surface
        border.width: field.activeFocus ? 2 : 1
        border.color: field.activeFocus ? Theme.primary : Theme.border
    }
}
