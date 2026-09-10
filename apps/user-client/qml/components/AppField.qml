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

    Menu {
        id: editMenu
        MenuItem { text: "剪切"; enabled: field.selectedText.length > 0 && !field.readOnly && field.echoMode === TextInput.Normal; onTriggered: field.cut() }
        MenuItem { text: "复制"; enabled: field.selectedText.length > 0 && field.echoMode === TextInput.Normal; onTriggered: field.copy() }
        MenuItem { text: "粘贴"; enabled: field.canPaste && !field.readOnly; onTriggered: { field.forceActiveFocus(); field.paste() } }
        MenuItem { text: "全选"; enabled: field.text.length > 0; onTriggered: field.selectAll() }
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: { field.forceActiveFocus(); editMenu.popup() }
    }
}
