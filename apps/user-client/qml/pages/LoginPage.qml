import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser
import "../components"

Item {
    id: page
    signal completed()

    Connections {
        target: appController
        function onLoginSucceeded() { page.completed() }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: content.implicitHeight + 80
        clip: true
        ColumnLayout {
            id: content
            width: Math.min(parent.width - 44, 430)
            anchors.horizontalCenter: parent.horizontalCenter
            y: 46
            spacing: 14

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 88; height: 88; radius: 28
                gradient: Gradient {
                    GradientStop { position: 0; color: "#22C9B5" }
                    GradientStop { position: 1; color: Theme.primaryDark }
                }
                Text {
                    anchors.centerIn: parent
                    text: "ϟ"
                    color: "white"
                    font.pixelSize: 52
                    font.bold: true
                }
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                text: "充电客户端"
                color: Theme.text
                font.pixelSize: 30
                font.bold: true
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "便捷找桩 · 安心充电"
                color: Theme.textMuted
                font.pixelSize: 14
            }

            AppCard {
                Layout.fillWidth: true
                Layout.topMargin: 18
                implicitHeight: 178
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12
                    Text { text: "手机号登录"; color: Theme.text; font.pixelSize: 16; font.bold: true }
                    TextField {
                        id: phoneInput
                        Layout.fillWidth: true
                        implicitHeight: 50
                        placeholderText: "请输入 11 位手机号"
                        text: appController.lastPhone
                        inputMethodHints: Qt.ImhDigitsOnly
                        maximumLength: 11
                        font.pixelSize: 16
                        background: Rectangle {
                            radius: 12
                            color: "#F8FAFC"
                            border.width: phoneInput.activeFocus ? 2 : 1
                            border.color: phoneInput.activeFocus ? Theme.primary : Theme.border
                        }
                        onAccepted: appController.login(text)
                    }
                    AppButton {
                        Layout.fillWidth: true
                        text: appController.busy ? "登录中…" : "登录"
                        enabled: appController.connected && !appController.busy
                        onClicked: appController.login(phoneInput.text)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
                Text { text: "演示账号"; color: Theme.textMuted; font.pixelSize: 12 }
                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 10
                rowSpacing: 10
                Repeater {
                    model: [
                        { label: "余额充足", phone: "18800000001", color: "#E7F8F1" },
                        { label: "待结算", phone: "18800000002", color: "#FFF3DB" },
                        { label: "低余额", phone: "18800000003", color: "#EDF3FF" },
                        { label: "已冻结", phone: "18800000004", color: "#FDEBEC" }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 58
                        radius: 12
                        color: modelData.color
                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: Theme.text; font.bold: true; font.pixelSize: 12 }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.phone; color: Theme.textMuted; font.pixelSize: 11 }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: phoneInput.text = modelData.phone
                        }
                    }
                }
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4
                spacing: 7
                Rectangle {
                    width: 8; height: 8; radius: 4
                    color: appController.connected ? Theme.success : Theme.warning
                }
                Text {
                    text: appController.connected ? "服务已连接" : "正在连接服务…"
                    color: Theme.textMuted
                    font.pixelSize: 12
                }
            }
        }
    }
}
