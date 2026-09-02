import QtQuick
import ChargingUser

Rectangle {
    id: badge
    property string status: "idle"
    property string label: status === "idle" ? "闲置"
                           : status === "charging" ? "使用中"
                           : status === "fault" ? "故障"
                           : status === "offline" ? "离线"
                           : status === "reserved" ? "已预约"
                           : status === "awaiting_payment" ? "待结算"
                           : status === "completed" ? "已结算" : status
    implicitWidth: textItem.implicitWidth + 20
    implicitHeight: 28
    radius: 14
    color: status === "idle" || status === "completed" ? "#E7F8F1"
           : status === "charging" || status === "awaiting_payment" ? "#FFF3DB"
           : status === "fault" ? "#FDEBEC" : "#EDF1F5"
    Text {
        id: textItem
        anchors.centerIn: parent
        text: badge.label
        font.pixelSize: 12
        font.bold: true
        color: badge.status === "idle" || badge.status === "completed" ? "#11845B"
               : badge.status === "charging" || badge.status === "awaiting_payment" ? "#B56500"
               : badge.status === "fault" ? Theme.danger : Theme.textMuted
    }
}
