import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser
import "../components"

Item {
    id: page
    property var order: appController.activeOrder

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            Column {
                spacing: 2
                Text { text: "充电中心"; color: Theme.text; font.pixelSize: 25; font.bold: true }
                Text { text: order.id ? "实时掌握当前订单" : "预约电桩后在此开始充电"; color: Theme.textMuted; font.pixelSize: 12 }
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 48; height: 48; radius: 16; color: Theme.primarySoft
                Text { anchors.centerIn: parent; text: "ϟ"; color: Theme.primary; font.pixelSize: 28; font.bold: true }
            }
        }

        AppCard {
            Layout.fillWidth: true
            implicitHeight: order.id ? 250 : 220
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 12
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 92; height: 92; radius: 46
                    color: order.status === "charging" ? Theme.primarySoft : "#F1F5F9"
                    Rectangle {
                        anchors.centerIn: parent
                        width: 70; height: 70; radius: 35
                        color: order.status === "charging" ? Theme.primary : Theme.surface
                        border.width: 2
                        border.color: order.status === "charging" ? Theme.primary : Theme.border
                        Text {
                            anchors.centerIn: parent
                            text: order.id ? "ϟ" : "○"
                            color: order.status === "charging" ? "white" : Theme.textMuted
                            font.pixelSize: 38
                            font.bold: true
                        }
                    }
                    SequentialAnimation on scale {
                        running: order.status === "charging"
                        loops: Animation.Infinite
                        NumberAnimation { to: 1.08; duration: 900; easing.type: Easing.InOutQuad }
                        NumberAnimation { to: 1.0; duration: 900; easing.type: Easing.InOutQuad }
                    }
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: !order.id ? "暂无进行中的订单"
                          : order.status === "reserved" ? "电桩已预约"
                          : order.status === "charging" ? "正在充电"
                          : order.status === "awaiting_payment" ? "等待结算" : order.status
                    color: Theme.text
                    font.pixelSize: 21
                    font.bold: true
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: order.id
                    text: "订单 #" + (order.id || "") + " · " + appController.chargingEstimate
                    color: Theme.textMuted
                    font.pixelSize: 13
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: !order.id
                    text: "请前往首页选择空闲电桩"
                    color: Theme.textMuted
                    font.pixelSize: 13
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            visible: order.id
            columns: 2
            columnSpacing: 10
            rowSpacing: 10
            Repeater {
                model: [
                    { value: Number(order.energy_kwh || 0).toFixed(3), label: "已充电量 kWh" },
                    { value: "￥" + Number(order.amount || 0).toFixed(2), label: "当前金额" }
                ]
                delegate: AppCard {
                    Layout.fillWidth: true
                    implicitHeight: 92
                    Column {
                        anchors.centerIn: parent
                        spacing: 4
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.value; color: Theme.primaryDark; font.pixelSize: 22; font.bold: true }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: Theme.textMuted; font.pixelSize: 11 }
                    }
                }
            }
        }
        Item { Layout.fillHeight: true }
        AppButton {
            Layout.fillWidth: true
            visible: order.status === "reserved"
            text: "开始充电"
            onClicked: appController.orderAction("order.start")
        }
        AppButton {
            Layout.fillWidth: true
            visible: order.status === "reserved"
            text: "取消预约"
            variant: "danger"
            onClicked: appController.orderAction("order.cancel")
        }
        AppButton {
            Layout.fillWidth: true
            visible: order.status === "charging"
            text: "停止充电"
            variant: "danger"
            onClicked: appController.orderAction("order.stop")
        }
        AppButton {
            Layout.fillWidth: true
            visible: order.status === "awaiting_payment"
            text: "钱包结算 ￥" + Number(order.amount || 0).toFixed(2)
            onClicked: appController.orderAction("order.settle")
        }
    }
}
