import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser
import "../components"

Item {
    id: root

    // 页面打开时刷新用户资料和订单历史
    Component.onCompleted: {
        appController.refreshProfile()
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: root.width
            spacing: 14

            Item {
                Layout.fillWidth: true
                height: 8
            }

            // 页面标题
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18

                ColumnLayout {
                    spacing: 3

                    Text {
                        text: "我的订单"
                        color: Theme.text
                        font.pixelSize: 24
                        font.bold: true
                    }

                    Text {
                        text: "查看充电订单和消费记录"
                        color: Theme.textMuted
                        font.pixelSize: 12
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    text: appController.history.length + " 条订单"
                    color: Theme.textMuted
                    font.pixelSize: 12
                }
            }

            // 当前进行中的订单
            AppCard {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                visible: Object.keys(appController.activeOrder).length > 0
                implicitHeight: 180

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "当前订单"
                            color: Theme.text
                            font.pixelSize: 16
                            font.bold: true
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        StatusBadge {
                            status: appController.activeOrder.status
                        }
                    }

                    Text {
                        text: "订单 #" + (appController.activeOrder.id || "--")
                        color: Theme.textMuted
                        font.pixelSize: 12
                    }

                    Text {
                        text: (appController.activeOrder.station_name || "当前充电站")
                              + " · "
                              + (appController.activeOrder.pile_code || "当前电桩")
                        color: Theme.text
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.border
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        ColumnLayout {
                            Text {
                                text: Number(appController.activeOrder.energy_kwh || 0).toFixed(3)
                                       + " kWh"
                                color: Theme.text
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Text {
                                text: "充电量"
                                color: Theme.textMuted
                                font.pixelSize: 11
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        ColumnLayout {
                            Text {
                                Layout.alignment: Qt.AlignRight
                                text: "￥"
                                      + Number(appController.activeOrder.amount || 0).toFixed(2)
                                color: Theme.primaryDark
                                font.pixelSize: 18
                                font.bold: true
                            }

                            Text {
                                Layout.alignment: Qt.AlignRight
                                text: "当前费用"
                                color: Theme.textMuted
                                font.pixelSize: 11
                            }
                        }
                    }

                    AppButton {
                        Layout.fillWidth: true
                        text: appController.activeOrder.status === "awaiting_payment"
                              ? "去结算"
                              : "查看充电详情"

                        onClicked: {
                            app.showCharging()
                        }
                    }
                }
            }

            // 历史订单标题
            Text {
                Layout.leftMargin: 18
                Layout.topMargin: 4

                text: "历史订单"
                color: Theme.text
                font.pixelSize: 16
                font.bold: true
            }

            // 历史订单列表
            Repeater {
                model: appController.history

                delegate: AppCard {
                    Layout.fillWidth: true
                    Layout.leftMargin: 18
                    Layout.rightMargin: 18
                    implicitHeight: 155

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                Layout.fillWidth: true
                                text: "订单 #" + modelData.id
                                color: Theme.text
                                font.pixelSize: 15
                                font.bold: true
                            }

                            StatusBadge {
                                status: modelData.status
                            }
                        }

                        Text {
                            text: (modelData.station_name || "充电站")
                                  + " · "
                                  + (modelData.pile_code || "电桩")
                            color: Theme.textMuted
                            font.pixelSize: 12
                        }

                        Text {
                            text: "开始时间："
                                  + String(
                                      modelData.started_at
                                      || modelData.created_at
                                      || "--"
                                  ).replace("T", " ").slice(0, 19)
                            color: Theme.textMuted
                            font.pixelSize: 11
                        }

                        Text {
                            visible: modelData.ended_at !== undefined
                                     && modelData.ended_at !== ""
                            text: "结束时间："
                                  + String(modelData.ended_at || "--")
                                    .replace("T", " ")
                                    .slice(0, 19)
                            color: Theme.textMuted
                            font.pixelSize: 11
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Theme.border
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: Number(modelData.energy_kwh || 0).toFixed(3)
                                      + " kWh"
                                color: Theme.text
                                font.pixelSize: 13
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Text {
                                text: "￥" + Number(modelData.amount || 0).toFixed(2)
                                color: Theme.primaryDark
                                font.pixelSize: 17
                                font.bold: true
                            }
                        }
                    }
                }
            }

            // 没有历史订单时显示
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: 30
                visible: appController.history.length === 0
                spacing: 8

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "○"
                    color: Theme.textMuted
                    font.pixelSize: 34
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "暂无订单记录"
                    color: Theme.textMuted
                    font.pixelSize: 14
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "完成一次充电后，订单会显示在这里"
                    color: Theme.textMuted
                    font.pixelSize: 11
                }
            }

            Item {
                Layout.fillWidth: true
                height: 24
            }
        }
    }
}