import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser
import "../components"

Item {
    id: page
    signal loggedOut()

    Component.onCompleted: appController.refreshProfile()

    AppDialog {
        id: rechargeDialog
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 340)
        modal: true
        title: "钱包充值"
        acceptText: "确认充值"
        onAccepted: appController.recharge(Number(amountInput.text))
        contentItem: ColumnLayout {
            spacing: 10
            Text { text: "请输入充值金额（元）"; color: Theme.text }
            AppField {
                id: amountInput
                Layout.fillWidth: true; Layout.minimumWidth: 0
                text: "100"
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                selectByMouse: true
            }
        }
    }

    AppDialog {
        id: logoutDialog
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 340)
        modal: true
        title: "退出登录"
        acceptText: "退出登录"
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

    AppScrollView {
        id: profileScroll
        anchors.fill: parent
        contentWidth: availableWidth
        ColumnLayout {
            width: profileScroll.availableWidth
            spacing: 14
            anchors.margins: 18

            RowLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.topMargin: 18
                Text { text: "个人中心"; color: Theme.text; font.pixelSize: 25; font.bold: true }
                Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                Rectangle {
                    width: 42; height: 42; radius: 12; color: Theme.primarySoft
                    AppIcon { anchors.centerIn: parent; name: "person"; iconColor: Theme.primary; width: 22; height: 22 }
                }
            }

            AppCard {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                implicitHeight: 250
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 16
                    // 头像:已设置 avatar_path 时显示圆形图片,否则回退昵称首字母;
                    // 点击任意处打开文件选择器更换(预览即时生效)
                    Item {
                        width: 76; height: 76
                        property bool hasAvatar: String(appController.user.avatar_path || "").length > 0
                        Rectangle {
                            anchors.fill: parent
                            radius: 38
                            visible: avatar.status !== Image.Ready
                            color: Theme.primaryDark
                            Text {
                                anchors.centerIn: parent
                                text: String(appController.user.nickname || "用户").slice(0, 1)
                                color: "white"
                                font.pixelSize: 30
                                font.bold: true
                            }
                        }
                        Image {
                            id: avatar
                            anchors.fill: parent
                            visible: status === Image.Ready
                            source: parent.hasAvatar ? "file:///" + String(appController.user.avatar_path).replace(/\\/g, "/").replace(/^\//, "") : ""
                            fillMode: Image.PreserveAspectFit
                            // 头像会被用户频繁更换，关闭组件缓存可避免继续显示旧像素。
                            // 文件名时间戳负责改变 source，本设置作为额外保障。
                            cache: false
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            width: 24; height: 24; radius: 12
                            color: Theme.primary
                            border.width: 2; border.color: "white"
                            AppIcon { anchors.centerIn: parent; name: "pen"; iconColor: "white"; width: 13; height: 13 }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            enabled: !appController.busy
                            onClicked: appController.pickAvatar()
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        spacing: 6
                        AppField {
                            id: nicknameInput
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            text: appController.user.nickname || ""
                            font.pixelSize: 17
                            font.bold: true

                        }
                        Text { text: appController.user.phone || ""; color: Theme.textMuted; font.pixelSize: 13 }
                        Text { text: "注册于 " + appController.displayTime(appController.user.created_at || "--"); color: Theme.textMuted; font.pixelSize: 11 }
                        Text {
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            text: nicknameInput.text.trim().length === 0 ? "昵称不能为空" : Array.from(nicknameInput.text.trim()).length > 30 ? "昵称不能超过30字" : ""
                            visible: text.length > 0
                            wrapMode: Text.Wrap
                            color: Theme.danger
                            font.pixelSize: 12
                        }
                        AppButton {
                            implicitHeight: 34
                            enabled: !appController.busy && nicknameInput.text.trim().length > 0 && Array.from(nicknameInput.text.trim()).length <= 30
                            text: "保存昵称"
                            variant: "secondary"
                            onClicked: appController.updateNickname(nicknameInput.text)
                        }
                        AppButton {
                            implicitHeight: 30
                            text: "取消修改"
                            variant: "secondary"
                            enabled: !appController.busy
                            onClicked: nicknameInput.text = appController.user.nickname || ""
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Text { text: "界面主题"; font.bold: true; color: Theme.text; font.pixelSize: 16 }
                RowLayout {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    Repeater {
                        model: [{name:"信号蓝", key:"default"}, {name:"云白蓝", key:"porcelain"}, {name:"翡翠绿", key:"emerald"}]
                        delegate: AppButton {
                            required property var modelData
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            leftPadding: 6; rightPadding: 6
                            font.pixelSize: 13
                            text: modelData.name
                            variant: appController.theme === modelData.key ? "primary" : "secondary"
                            onClicked: appController.theme = modelData.key
                        }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                implicitHeight: 112
                radius: Theme.radius
                color: Theme.primaryDark
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    Column {
                        spacing: 5
                        Text { text: "钱包余额"; color: "#CCFFFFFF"; font.pixelSize: 12 }
                        Text { text: "￥" + Number(appController.user.wallet_balance || 0).toFixed(2); color: "white"; font.pixelSize: 30; font.bold: true }
                    }
                    Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                    Rectangle {
                        width: 74; height: 40; radius: 12
                        color: "white"
                        Text { anchors.centerIn: parent; text: "充值"; color: Theme.primaryDark; font.bold: true }
                        MouseArea { anchors.fill: parent; enabled: !appController.busy; onClicked: rechargeDialog.open(); cursorShape: Qt.PointingHandCursor }
                    }
                }
            }
            Text {
    Layout.leftMargin: 18
    Layout.topMargin: 8
    text: "充值记录"
    color: Theme.text
    font.pixelSize: 16
    font.bold: true
}

Repeater {
    model: appController.rechargeHistory

    delegate: AppCard {
        Layout.fillWidth: true; Layout.minimumWidth: 0
        Layout.leftMargin: 18
        Layout.rightMargin: 18
        implicitHeight: 108

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 6

            RowLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0

                Text {
                    text: "+￥" + Number(modelData.amount || 0).toFixed(2)
                    color: Theme.primaryDark
                    font.pixelSize: 18
                    font.bold: true
                }

                Item {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                }

                Text {
                    text: appController.displayTime(modelData.created_at || "--")
                    color: Theme.textMuted
                    font.pixelSize: 11
                }
            }

            Text {
                text: "充值前：￥"
                      + Number(modelData.balance_before || 0).toFixed(2)
                      + "  →  充值后：￥"
                      + Number(modelData.balance_after || 0).toFixed(2)
                color: Theme.textMuted
                font.pixelSize: 12
            }

            Text {
                text: "充值记录 #" + (modelData.id || "--")
                color: Theme.textMuted
                font.pixelSize: 11
            }
        }
    }
}

ColumnLayout {
    Layout.fillWidth: true; Layout.minimumWidth: 0
    Layout.topMargin: 20
    visible: appController.rechargeHistory.length === 0
    spacing: 6

    Text {
        Layout.alignment: Qt.AlignHCenter
        text: "暂无充值记录"
        color: Theme.textMuted
        font.pixelSize: 13
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        text: "完成充值后，记录会显示在这里"
        color: Theme.textMuted
        font.pixelSize: 11
    }
}

            RowLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Text { text: "充电订单"; color: Theme.text; font.pixelSize: 19; font.bold: true }
                Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                Text { text: appController.history.length + " 条"; color: Theme.textMuted; font.pixelSize: 11 }
            }
            Repeater {
                model: appController.history
                delegate: AppCard {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    Layout.leftMargin: 18
                    Layout.rightMargin: 18
                    implicitHeight: 142
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 5
                        RowLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            Text { Layout.fillWidth: true; Layout.minimumWidth: 0; text: "订单 #" + modelData.id; color: Theme.text; font.bold: true; font.pixelSize: 15 }
                            StatusBadge { status: modelData.status }
                        }
                        Text { Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight; text: (modelData.station_name || "充电站") + " · " + (modelData.pile_code || "电桩"); color: Theme.textMuted; font.pixelSize: 12 }
                        Text { text: "完成时间 " + appController.displayTime(modelData.ended_at || modelData.created_at || "--"); color: Theme.textMuted; font.pixelSize: 11 }
                        Rectangle { Layout.fillWidth: true; Layout.minimumWidth: 0; height: 1; color: Theme.border }
                        RowLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            Text { text: Number(modelData.energy_kwh || 0).toFixed(3) + " kWh"; color: Theme.text; font.pixelSize: 13 }
                            Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
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
                Layout.fillWidth: true; Layout.minimumWidth: 0
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
