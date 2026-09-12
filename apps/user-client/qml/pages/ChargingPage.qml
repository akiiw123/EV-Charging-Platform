import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser
import "../components"

Item {
    id: page
    property var order: appController.activeOrder

    AppScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
    ColumnLayout {
        width: page.width - 36
        x: 18
        spacing: 14
        RowLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            Column {
                spacing: 2
                Text { text: "充电中心"; color: Theme.text; font.pixelSize: 25; font.bold: true }
                Text { text: order.id ? "实时掌握当前订单" : "预约电桩后在此开始充电"; color: Theme.textMuted; font.pixelSize: 12 }
            }
            Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
        }

        AppCard {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            implicitHeight: 132
            Layout.topMargin: 8
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    spacing: 8
                    Rectangle {
                        width: 10; height: 10; radius: 5
                        visible: !!order.id
                        color: order.status === "charging" ? Theme.success
                              : order.status === "awaiting_payment" ? Theme.warning
                              : order.status === "reserved" ? Theme.primary : Theme.textMuted
                    }
                    Text {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        text: !order.id ? "暂无进行中的订单" : appController.orderStatusText(order.status)
                        color: Theme.text
                        font.pixelSize: 20
                        font.bold: true
                    }
                }
                Text {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    visible: !!order.id
                    wrapMode: Text.Wrap
                    text: "订单 #" + (order.id || "") + " · " + appController.chargingEstimate
                    color: Theme.textMuted
                    font.pixelSize: 13
                }
                Text {
                    visible: !order.id
                    text: "请前往首页选择空闲电桩"
                    color: Theme.textMuted
                    font.pixelSize: 13
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            visible: !!order.id
            columns: 2
            columnSpacing: 10
            rowSpacing: 10
            Repeater {
                model: [
                    { value: Number(order.energy_kwh || 0).toFixed(3), label: "已充电量 kWh" },
                    { value: "￥" + Number(order.amount || 0).toFixed(2), label: "当前金额" }
                ]
                delegate: AppCard {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    implicitHeight: 80
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
            Layout.fillWidth: true; Layout.minimumWidth: 0
            visible: order.status === "reserved"
            text: "开始充电"
            enabled: !appController.busy
            onClicked: page.confirm("order.start", "开始充电")
        }
        AppButton {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            visible: order.status === "reserved"
            text: "取消预约"
            variant: "danger"
            enabled: !appController.busy
            onClicked: page.confirm("order.cancel", "取消预约")
        }
        AppButton {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            visible: order.status === "charging"
            text: "停止充电"
            variant: "danger"
            enabled: !appController.busy
            onClicked: page.confirm("order.stop", "停止充电")
        }
        AppButton {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            visible: order.status === "awaiting_payment"
            text: "钱包结算 ￥" + Number(order.amount || 0).toFixed(2)
            enabled: !appController.busy
            onClicked: page.confirm("order.settle", "结算")
        }
    }
    }
    property string pendingAction: ""
    property var confirmedOrderId: 0
    function confirm(action, label) {
        pendingAction = action
        confirmedOrderId = order.id
        confirmDialog.title = "确认" + label
        confirmDialog.open()
    }
    AppDialog {
        id: confirmDialog
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 340)
        modal: true
        acceptText: "确认"
        contentItem: Label { text: "是否对订单 #" + page.confirmedOrderId + " 执行此操作？"; wrapMode: Text.Wrap }
        onAccepted: { if (order.id === page.confirmedOrderId) appController.orderAction(page.pendingAction) }
    }
    AppDialog {
        id: rechargeDialog
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 340)
        modal: true
        title: "余额不足，请先充值"
        acceptText: "确认充值"
        contentItem: ColumnLayout {
            Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: "充值成功后将再次确认结算。"; wrapMode: Text.Wrap }
            AppField { id: rechargeAmount; Layout.fillWidth: true; Layout.minimumWidth: 0; text: "100"; placeholderText: "充值金额（元）"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
        }
        onAccepted: appController.recharge(Number(rechargeAmount.text))
    }
    Connections {
        target: appController
        function onRechargeRequired() { rechargeDialog.open() }
        function onRechargeSucceeded() { if (order.status === "awaiting_payment") page.confirm("order.settle", "重新结算") }
    }
}
