/*
 * 文件职责：共享文本框：向用户端和管理端提供统一输入视觉及焦点状态。
 * 对接关系：属于 Charging.UI 共享设计系统；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick

FocusScope {
    id: control
    property alias text: input.text
    property alias echoMode: input.echoMode
    property alias validator: input.validator
    property string placeholderText: ""
    property bool error: false
    signal accepted()
    implicitHeight: Theme.controlHeight
    implicitWidth: 200
    activeFocusOnTab: true
    onActiveFocusChanged: if (activeFocus) input.forceActiveFocus()
    Rectangle {
        anchors.fill: parent; radius: Theme.radiusSmall; color: Theme.backgroundSecondary
        border.width: input.activeFocus || control.error ? 2 : 1
        border.color: control.error ? Theme.danger : input.activeFocus ? Theme.focusRing : Theme.borderSubtle
        Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }
    }
    Text {
        anchors.left: parent.left; anchors.leftMargin: 13; anchors.right: parent.right; anchors.rightMargin: 13; anchors.verticalCenter: parent.verticalCenter
        text: control.placeholderText; color: Theme.textMuted; font.pixelSize: Theme.fontBody; visible: input.text.length === 0 && !input.activeFocus; elide: Text.ElideRight
    }
    TextInput {
        id: input; anchors.fill: parent; anchors.leftMargin: 13; anchors.rightMargin: 13
        verticalAlignment: TextInput.AlignVCenter; color: Theme.textPrimary; selectionColor: Theme.accent; selectedTextColor: "white"
        font.pixelSize: Theme.fontBody; clip: true; onAccepted: control.accepted()
    }
}
