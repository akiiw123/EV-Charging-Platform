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
        id: loginScroll
        WheelArea { flickable: loginScroll }
        anchors.fill: parent
        contentHeight: content.implicitHeight + 80
        clip: true
        ColumnLayout {
            id: content
            width: Math.min(parent.width - 44, 430)
            anchors.horizontalCenter: parent.horizontalCenter
            y: 46
            spacing: 14

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 26
                text: "充电"
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
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.topMargin: 18
                implicitHeight: 178
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12
                    Text { text: "手机号登录"; color: Theme.text; font.pixelSize: 16; font.bold: true }
                    AppField {
                        id: phoneInput
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        implicitHeight: 50
                        placeholderText: "请输入 11 位手机号"
                        text: appController.lastPhone
                        inputMethodHints: Qt.ImhDigitsOnly
                        maximumLength: 11
                        font.pixelSize: 16
                        background: Rectangle {
                            radius: 12
                            color: Theme.backgroundSecondary
                            border.width: phoneInput.activeFocus ? 2 : 1
                            border.color: phoneInput.activeFocus ? Theme.primary : Theme.border
                        }
                        onAccepted: appController.login(text)
                    }
                    AppButton {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        text: appController.busy ? "登录中…" : "登录"
                        enabled: appController.connected && !appController.busy
                        onClicked: appController.login(phoneInput.text)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.topMargin: 8
                Rectangle { Layout.fillWidth: true; Layout.minimumWidth: 0; height: 1; color: Theme.border }
                Text { text: "演示账号"; color: Theme.textMuted; font.pixelSize: 12 }
                Rectangle { Layout.fillWidth: true; Layout.minimumWidth: 0; height: 1; color: Theme.border }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                spacing: 2
                Repeater {
                    model: [
                        { label: "余额充足", phone: "18800000001" },
                        { label: "待结算", phone: "18800000002" },
                        { label: "低余额", phone: "18800000003" },
                        { label: "已冻结", phone: "18800000004" }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        implicitHeight: 40
                        color: demoMouse.containsMouse ? Theme.primarySoft : "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            Text { text: modelData.label; color: Theme.text; font.pixelSize: 13 }
                            Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                            Text { text: modelData.phone; color: Theme.textMuted; font.pixelSize: 12 }
                        }
                        MouseArea {
                            id: demoMouse
                            anchors.fill: parent
                            hoverEnabled: true
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
