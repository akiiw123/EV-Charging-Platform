import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser
import "../components"

Item {
    id: page
    signal loggedOut()

    // 昵称编辑态:点击铅笔进入,保存/取消退出,避免编辑控件常驻
    property bool editingNickname: false

    // 本地统计:已完成订单数与累计消费(与订单列表同源)
    readonly property int completedCount: {
        var n = 0
        for (var i = 0; i < appController.history.length; ++i)
            if (appController.history[i].status === "completed") ++n
        return n
    }
    readonly property double totalSpent: {
        var sum = 0
        for (var i = 0; i < appController.history.length; ++i)
            if (appController.history[i].status === "completed")
                sum += Number(appController.history[i].amount || 0)
        return sum
    }
    readonly property int rechargeCount: appController.rechargeHistory.length

    Component.onCompleted: appController.refreshProfile()

    function startNicknameEdit() {
        nicknameInput.text = appController.user.nickname || ""
        editingNickname = true
    }

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
        anchors.fill: parent
        contentWidth: availableWidth
        ColumnLayout {
            width: page.width - 36
            x: 18
            spacing: 14

            RowLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Text { text: "个人中心"; color: Theme.text; font.pixelSize: 25; font.bold: true }
                Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
            }

            // 资料卡:头像 + 昵称(铅笔进入编辑) + 账号信息 + 消费统计
            AppCard {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                implicitHeight: page.editingNickname ? 286 : 196
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        spacing: 14
                        Item {
                            width: 72; height: 72
                            property bool hasAvatar: String(appController.user.avatar_path || "").length > 0
                            Rectangle {
                                anchors.fill: parent
                                radius: 36
                                visible: avatar.status !== Image.Ready
                                color: Theme.primaryDark
                                Text {
                                    anchors.centerIn: parent
                                    text: String(appController.user.nickname || "用户").slice(0, 1)
                                    color: "white"
                                    font.pixelSize: 28
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
                                width: 22; height: 22; radius: 11
                                color: Theme.primary
                                border.width: 2; border.color: Theme.surface
                                AppIcon { anchors.centerIn: parent; name: "pen"; iconColor: "white"; width: 12; height: 12 }
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
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                visible: !page.editingNickname
                                Text {
                                    Layout.fillWidth: true; Layout.minimumWidth: 0
                                    text: appController.user.nickname || "用户"
                                    color: Theme.text
                                    font.pixelSize: 18
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Rectangle {
                                    width: 28; height: 28; radius: 6
                                    color: Theme.primarySoft
                                    AppIcon { anchors.centerIn: parent; name: "pen"; iconColor: Theme.primary; width: 14; height: 14 }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: page.startNicknameEdit()
                                    }
                                }
                            }
                            AppField {
                                id: nicknameInput
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                visible: page.editingNickname
                                text: appController.user.nickname || ""
                                font.pixelSize: 16
                                font.bold: true
                                maximumLength: 30
                            }
                            Text {
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                visible: page.editingNickname
                                text: nicknameInput.text.trim().length === 0 ? "昵称不能为空"
                                      : Array.from(nicknameInput.text.trim()).length > 30 ? "昵称不能超过30字" : ""
                                color: Theme.danger
                                font.pixelSize: 12
                            }
                            Text {
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                text: appController.user.phone || ""
                                color: Theme.textMuted
                                font.pixelSize: 13
                            }
                            Text {
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                text: "注册于 " + appController.displayTime(appController.user.created_at || "--")
                                color: Theme.textMuted
                                font.pixelSize: 11
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        visible: page.editingNickname
                        spacing: 10
                        AppButton {
                            implicitHeight: 36
                            enabled: !appController.busy && nicknameInput.text.trim().length > 0 && Array.from(nicknameInput.text.trim()).length <= 30
                            text: "保存昵称"
                            onClicked: appController.updateNickname(nicknameInput.text)
                        }
                        AppButton {
                            implicitHeight: 36
                            text: "取消"
                            variant: "secondary"
                            enabled: !appController.busy
                            onClicked: page.editingNickname = false
                        }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.minimumWidth: 0; height: 1; color: Theme.border }

                    RowLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        ColumnLayout {
                            spacing: 2
                            Text { text: page.completedCount + " 次"; color: Theme.text; font.pixelSize: 17; font.bold: true }
                            Text { text: "充电次数"; color: Theme.textMuted; font.pixelSize: 11 }
                        }
                        Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        Rectangle { width: 1; height: 30; color: Theme.border }
                        Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        ColumnLayout {
                            spacing: 2
                            Text { text: "￥" + page.totalSpent.toFixed(2); color: Theme.text; font.pixelSize: 17; font.bold: true }
                            Text { text: "累计消费"; color: Theme.textMuted; font.pixelSize: 11 }
                        }
                        Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        Rectangle { width: 1; height: 30; color: Theme.border }
                        Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        ColumnLayout {
                            spacing: 2
                            Text { text: "￥" + Number(appController.user.wallet_balance || 0).toFixed(2); color: Theme.text; font.pixelSize: 17; font.bold: true }
                            Text { text: "钱包余额"; color: Theme.textMuted; font.pixelSize: 11 }
                        }
                        Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                    }
                }
            }

            // 钱包卡:余额与充值入口
            Rectangle {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                implicitHeight: 112
                radius: Theme.radiusLarge
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
                        width: 74; height: 40; radius: 6
                        color: "white"
                        Text { anchors.centerIn: parent; text: "充值"; color: Theme.primaryDark; font.bold: true }
                        MouseArea { anchors.fill: parent; enabled: !appController.busy; onClicked: rechargeDialog.open(); cursorShape: Qt.PointingHandCursor }
                    }
                }
            }

            // 界面主题
            AppCard {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                implicitHeight: 104
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10
                    Text { text: "界面主题"; color: Theme.text; font.pixelSize: 15; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        spacing: 10
                        Repeater {
                            model: [{name:"信号蓝", key:"default"}, {name:"云白蓝", key:"porcelain"}, {name:"翡翠绿", key:"emerald"}]
                            delegate: AppButton {
                                required property var modelData
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                implicitHeight: 38
                                leftPadding: 6; rightPadding: 6
                                font.pixelSize: 13
                                text: modelData.name
                                variant: appController.theme === modelData.key ? "primary" : "secondary"
                                onClicked: appController.theme = modelData.key
                            }
                        }
                    }
                }
            }

            // 充值记录:单卡分组,空状态居中展示
            AppCard {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                implicitHeight: 56 + (page.rechargeCount > 0 ? page.rechargeCount * 62 : 160)
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        Text { text: "充值记录"; color: Theme.text; font.pixelSize: 15; font.bold: true }
                        Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        Text { text: page.rechargeCount + " 笔"; color: Theme.textMuted; font.pixelSize: 11 }
                    }
                    Repeater {
                        model: appController.rechargeHistory
                        delegate: ColumnLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                Text {
                                    text: "+￥" + Number(modelData.amount || 0).toFixed(2)
                                    color: Theme.success
                                    font.pixelSize: 16
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                                Text {
                                    text: appController.displayTime(modelData.created_at || "--")
                                    color: Theme.textMuted
                                    font.pixelSize: 11
                                }
                            }
                            Text {
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                                text: "￥" + Number(modelData.balance_before || 0).toFixed(2)
                                      + "  →  ￥" + Number(modelData.balance_after || 0).toFixed(2)
                                color: Theme.textMuted
                                font.pixelSize: 12
                            }
                            Rectangle { Layout.fillWidth: true; Layout.minimumWidth: 0; height: 1; color: Theme.border }
                        }
                    }
                    EmptyState {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        visible: page.rechargeCount === 0
                        icon: "wallet"
                        title: "暂无充值记录"
                        hint: "完成充值后，记录会显示在这里"
                    }
                }
            }

            // 充电订单
            RowLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.topMargin: 4
                Text { text: "充电订单"; color: Theme.text; font.pixelSize: 19; font.bold: true }
                Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                Text { text: appController.history.length + " 条"; color: Theme.textMuted; font.pixelSize: 11 }
            }
            Repeater {
                model: appController.history
                delegate: AppCard {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
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
            EmptyState {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                visible: appController.history.length === 0
                icon: "order"
                title: "暂无订单记录"
                hint: "完成一次充电后，订单会显示在这里"
            }
            AppButton {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.bottomMargin: 24
                text: "退出登录"
                variant: "danger"
                onClicked: logoutDialog.open()
            }
        }
    }
}
