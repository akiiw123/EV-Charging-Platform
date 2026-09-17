/*
 * 文件职责：订单页：展示活动订单和历史订单；只负责状态映射与交互，不在 QML 中计算最终账单。
 * 对接关系：属于 UserAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
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

    AppScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: root.width
            spacing: 14

            Item {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                height: 8
            }

            // 页面标题
            RowLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0
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
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                }

                Text {
                    text: appController.history.length + " 条订单"
                    color: Theme.textMuted
                    font.pixelSize: 12
                }
            }

            Label { Layout.leftMargin: 18; visible: !appController.activeOrder.id; text: "暂无当前订单"; color: Theme.textMuted }

            // 当前进行中的订单
            AppCard {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                visible: Object.keys(appController.activeOrder).length > 0
                implicitHeight: 240

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0

                        Text {
                            text: "当前订单"
                            color: Theme.text
                            font.pixelSize: 16
                            font.bold: true
                        }

                        Item {
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                        }

                        StatusBadge {
                            status: appController.activeOrder.status || ""
                        }
                    }

                    Text {
                        text: "订单 #" + (appController.activeOrder.id || "--")
                        color: Theme.textMuted
                        font.pixelSize: 12
                    }

                    Text {
                        Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight; text: (appController.activeOrder.station_name || "当前充电站")
                              + " · "
                              + (appController.activeOrder.pile_code || "当前电桩")
                        color: Theme.text
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Rectangle {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        height: 1
                        color: Theme.border
                    }

                    RowLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0

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
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                        }

                        ColumnLayout {
                            Text {
                                Layout.alignment: Qt.AlignRight
                                // 应付合计 = 电费 amount + 占位费明细 occupancy_fee
                                text: "￥"
                                      + Number((appController.activeOrder.amount || 0)
                                               + (appController.activeOrder.occupancy_fee || 0)).toFixed(2)
                                color: Theme.primaryDark
                                font.pixelSize: 18
                                font.bold: true
                            }

                            Text {
                                Layout.alignment: Qt.AlignRight
                                text: Number(appController.activeOrder.occupancy_fee || 0) > 0
                                      ? "当前费用(含占位费 ￥" + Number(appController.activeOrder.occupancy_fee).toFixed(2) + ")"
                                      : "当前费用"
                                color: Theme.textMuted
                                font.pixelSize: 11
                            }
                        }
                    }

                    AppButton {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
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
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    Layout.leftMargin: 18
                    Layout.rightMargin: 18
                    implicitHeight: 155

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: 0

                            Text {
                                Layout.fillWidth: true; Layout.minimumWidth: 0
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
                            Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight; text: (modelData.station_name || "充电站")
                                  + " · "
                                  + (modelData.pile_code || "电桩")
                            color: Theme.textMuted
                            font.pixelSize: 12
                        }

                        Text {
                            text: "开始时间："
                                  + appController.displayTime(
                                      modelData.started_at
                                      || modelData.created_at
                                      || "--"
                                  )
                            color: Theme.textMuted
                            font.pixelSize: 11
                        }

                        Text {
                            visible: modelData.ended_at !== undefined
                                     && modelData.ended_at !== ""
                            text: "结束时间："
                                  + appController.displayTime(modelData.ended_at || "--")
                            color: Theme.textMuted
                            font.pixelSize: 11
                        }

                        Rectangle {
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            height: 1
                            color: Theme.border
                        }

                        RowLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: 0

                            Text {
                                text: Number(modelData.energy_kwh || 0).toFixed(3)
                                      + " kWh"
                                color: Theme.text
                                font.pixelSize: 13
                            }

                            Item {
                                Layout.fillWidth: true; Layout.minimumWidth: 0
                            }

                            Text {
                                // 有占位费明细时一并展示:应付合计 = 电费 + 占位费
                                text: Number(modelData.occupancy_fee || 0) > 0
                                      ? "￥" + Number(modelData.amount || 0).toFixed(2)
                                        + "（含占位 ￥" + Number(modelData.occupancy_fee).toFixed(2) + "）"
                                      : "￥" + Number(modelData.amount || 0).toFixed(2)
                                color: Theme.primaryDark
                                font.pixelSize: 15
                                font.bold: true
                            }
                        }
                    }
                }
            }

            // 没有历史订单时显示统一的空状态
            EmptyState {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.topMargin: 12
                visible: appController.history.length === 0
                icon: "order"
                title: "暂无订单记录"
                hint: "完成一次充电后，订单会显示在这里"
            }

            Item {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                height: 24
            }
        }
    }
}
