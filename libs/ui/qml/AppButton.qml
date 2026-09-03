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
