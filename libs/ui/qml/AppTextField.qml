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
