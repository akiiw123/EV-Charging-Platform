import QtQuick

Rectangle {
    id: badge
    property string status: "idle"
    property string label: status
    readonly property color tone: status === "idle" || status === "active" || status === "completed" ? Theme.success
                                : status === "charging" ? Theme.accent
                                : status === "reserved" || status === "info" ? "#9B87F5"
                                : status === "fault" || status === "frozen" || status === "danger" ? Theme.danger
                                : status === "restarting" || status === "awaiting_payment" || status === "warning" ? Theme.warning
                                : Theme.textMuted
    implicitWidth: labelText.implicitWidth + 22
    implicitHeight: 26
    radius: 13
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.14)
    border.width: 1
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.35)
    Text { id: labelText; anchors.centerIn: parent; text: badge.label; color: badge.tone; font.pixelSize: Theme.fontCaption; font.weight: Font.DemiBold }
}
