/*
 * 文件职责：状态徽标：把协议状态映射为颜色、图标和中文文字，避免只靠颜色表达。
 * 对接关系：属于 Charging.UI 共享设计系统；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick

Rectangle {
    id: badge
    property string status: "idle"
    property string label: status
    readonly property color tone: status === "idle" || status === "active" || status === "completed" ? Theme.success
                                : status === "charging" ? Theme.info
                                : status === "reserved" || status === "info" ? "#9B87F5"
                                : status === "fault" || status === "frozen" || status === "danger" ? Theme.danger
                                : status === "restarting" || status === "awaiting_payment" || status === "warning" ? Theme.warning
                                : Theme.textMuted
    implicitWidth: labelText.implicitWidth + 22
    implicitHeight: 24
    radius: 4
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.14)
    border.width: 1
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.35)
    Text { id: labelText; anchors.centerIn: parent; text: badge.label; color: badge.tone; font.pixelSize: Theme.fontCaption; font.weight: Font.DemiBold }
}
