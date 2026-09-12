import QtQuick
import ChargingUser

// 与管理端 StatusBadge 相同的色调配方(半透明底 + 同色描边),标签保留中文映射
Rectangle {
    id: badge
    property string status: "idle"
    property string label: status === "idle" ? "闲置"
                           : status === "charging" ? "充电中"
                           : status === "fault" ? "故障"
                           : status === "offline" ? "离线"
                           : status === "reserved" ? "已预约"
                           : status === "awaiting_payment" ? "待结算"
                           : status === "completed" ? "已完成"
                           : status === "cancelled" ? "已取消" : "未知状态"
    readonly property color tone: status === "idle" || status === "active" || status === "completed" ? Theme.success
                                  : status === "charging" || status === "restarting" ? Theme.info
                                  : status === "reserved" || status === "info" ? "#9B87F5"
                                  : status === "fault" || status === "frozen" || status === "danger" ? Theme.danger
                                  : status === "awaiting_payment" || status === "warning" ? Theme.warning
                                  : Theme.textMuted
    implicitWidth: textItem.implicitWidth + 20
    implicitHeight: 24
    radius: 4
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.14)
    border.width: 1
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.35)
    Text {
        id: textItem
        anchors.centerIn: parent
        text: badge.label
        font.pixelSize: 12
        font.bold: true
        color: badge.tone
    }
}
