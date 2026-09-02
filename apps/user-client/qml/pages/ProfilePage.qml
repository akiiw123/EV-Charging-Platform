import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser
import "../components"

Item {
    id: page
    signal loggedOut()

    Component.onCompleted: appController.refreshProfile()

    Dialog {
        id: rechargeDialog
        anchors.centerIn: parent
        modal: true
        title: "钱包充值"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: appController.recharge(Number(amountInput.text))
        contentItem: ColumnLayout {
            spacing: 10
            Text { text: "请输入充值金额（元）"; color: Theme.text }
            TextField {
                id: amountInput
                text: "100"
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                selectByMouse: true
            }
        }
    }

    Dialog {
        id: logoutDialog
        anchors.centerIn: parent
        modal: true
        title: "退出登录"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            appController.logout()
            page.loggedOut()
        }
        contentItem: Text {
            text: "确定退出当前账号吗？"
            color: Theme.text
            wrapMode: Text.Wrap
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: 14
            anchors.margins: 18

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.topMargin: 18
                Text { text: "个人中心"; color: Theme.text; font.pixelSize: 25; font.bold: true }
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 42; height: 42; radius: 14; color: Theme.primarySoft
                    Text { anchors.centerIn: parent; text: "●"; color: Theme.primary; font.pixelSize: 20 }
                }
            }

            AppCard {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                implicitHeight: 172
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 16
                    Rectangle {
                        width: 76; height: 76; radius: 38
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#34D1BF" }
                            GradientStop { position: 1; color: Theme.primaryDark }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: String(appController.user.nickname || "用户").slice(0, 1)
                            color: "white"
                            font.pixelSize: 30
                            font.bold: true
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        TextField {
                            id: nicknameInput
                            Layout.fillWidth: true
                            text: appController.user.nickname || ""
                            font.pixelSize: 17
                            font.bold: true
                            background: Rectangle { color: "#F8FAFC"; radius: 10; border.color: Theme.border }
                        }
                        Text { text: appController.user.phone || ""; color: Theme.textMuted; font.pixelSize: 13 }
                        Text { text: "注册于 " + String(appController.user.created_at || "--").replace("T", " ").slice(0, 16); color: Theme.textMuted; font.pixelSize: 11 }
                        AppButton {
                            implicitHeight: 34
                            text: "保存昵称"
                            variant: "secondary"
                            onClicked: appController.updateNickname(nicknameInput.text)
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                implicitHeight: 112
                radius: Theme.radius
                gradient: Gradient {
                    GradientStop { position: 0; color: "#0B776D" }
                    GradientStop { position: 1; color: "#17AA99" }
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    Column {
                        spacing: 5
                        Text { text: "钱包余额"; color: "#CCFFFFFF"; font.pixelSize: 12 }
                        Text { text: "￥" + Number(appController.user.wallet_balance || 0).toFixed(2); color: "white"; font.pixelSize: 30; font.bold: true }
                    }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        width: 74; height: 40; radius: 12
                        color: "white"
                        Text { anchors.centerIn: parent; text: "充值"; color: Theme.primaryDark; font.bold: true }
                        MouseArea { anchors.fill: parent; onClicked: rechargeDialog.open(); cursorShape: Qt.PointingHandCursor }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Text { text: "充电订单"; color: Theme.text; font.pixelSize: 19; font.bold: true }
                Item { Layout.fillWidth: true }
                Text { text: appController.history.length + " 条"; color: Theme.textMuted; font.pixelSize: 11 }
            }
            Repeater {
                model: appController.history
                delegate: AppCard {
                    Layout.fillWidth: true
                    Layout.leftMargin: 18
                    Layout.rightMargin: 18
                    implicitHeight: 142
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 5
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: "订单 #" + modelData.id; color: Theme.text; font.bold: true; font.pixelSize: 15 }
                            StatusBadge { status: modelData.status }
                        }
                        Text { text: (modelData.station_name || "充电站") + " · " + (modelData.pile_code || "电桩"); color: Theme.textMuted; font.pixelSize: 12 }
                        Text { text: "完成时间 " + String(modelData.ended_at || modelData.created_at || "--").replace("T", " ").slice(0, 19); color: Theme.textMuted; font.pixelSize: 11 }
                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: Number(modelData.energy_kwh || 0).toFixed(3) + " kWh"; color: Theme.text; font.pixelSize: 13 }
                            Item { Layout.fillWidth: true }
                            Text { text: "￥" + Number(modelData.amount || 0).toFixed(2); color: Theme.primaryDark; font.pixelSize: 17; font.bold: true }
                        }
                    }
                }
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                visible: appController.history.length === 0
                text: "暂无订单记录"
                color: Theme.textMuted
                font.pixelSize: 13
            }
            AppButton {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.bottomMargin: 24
                text: "退出登录"
                variant: "danger"
                onClicked: logoutDialog.open()
            }
        }
    }
}
