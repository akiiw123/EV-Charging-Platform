import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser
import "../components"

Item {
    id: page
    signal back()
    signal openMap()
    signal openCharging()

    // 计价规则只读视图(来自 station.pricing);未拉到响应前回退固定单价
    readonly property var rule: appController.pricing.rule || null
    readonly property string ruleText: {
        if (!page.rule) return "本站未配置占位费规则，充电结束后不收取占位费。"
        var cap = Number(page.rule.occupancy_fee_cap) > 0
            ? "封顶 ￥" + Number(page.rule.occupancy_fee_cap).toFixed(2)
            : "不封顶"
        return "充满后未驶离：免费挪车 " + page.rule.free_move_minutes + " 分钟，超出按 ￥"
               + Number(page.rule.occupancy_fee_per_minute).toFixed(2)
               + "/分钟计占位费，" + cap + "，结算时并入待支付金额。"
    }
    function minuteRange(minute) {
        var h = Math.floor(minute / 60)
        var m = minute % 60
        return (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m
    }
    function periodLabel(type) { return type === "peak" ? "峰" : type === "flat" ? "平" : "谷" }

    Connections {
        target: appController
        function onReservationSucceeded() { page.openCharging() }
    }

    AppScrollView {
        id: detailScroll
        anchors.fill: parent
    ColumnLayout {
        width: detailScroll.availableWidth - 36
        x: 18
        spacing: 12
        RowLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            Rectangle {
                width: 42; height: 42; radius: 13; color: Theme.surface
                border.width: 1; border.color: Theme.border
                Text { anchors.centerIn: parent; text: "‹"; font.pixelSize: 30; color: Theme.text }
                MouseArea { anchors.fill: parent; onClicked: page.back(); cursorShape: Qt.PointingHandCursor }
            }
            Text {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                text: "电站详情"
                color: Theme.text
                font.pixelSize: 20
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }
            Item { width: 42; height: 42 }
        }
        AppCard {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            implicitHeight: detailContent.implicitHeight + 40
            ColumnLayout {
                id: detailContent
                anchors.fill: parent
                anchors.margins: 20
                spacing: 7
                Text { Layout.fillWidth: true; Layout.minimumWidth: 0; text: appController.selectedStation.name || ""; color: Theme.text; font.pixelSize: 22; font.bold: true; wrapMode: Text.Wrap }
                Text { Layout.fillWidth: true; Layout.minimumWidth: 0; text: appController.selectedStation.address || ""; color: Theme.textMuted; font.pixelSize: 13; wrapMode: Text.Wrap }
                Text {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    visible: (appController.selectedStation.province || "") !== ""
                    text: {
                        var parts = []
                        var s = appController.selectedStation
                        if (s.province) parts.push(s.province)
                        if (s.city && s.city !== s.province) parts.push(s.city)
                        if (s.district) parts.push(s.district)
                        return parts.join(" / ")
                    }
                    color: Theme.textMuted; font.pixelSize: 13
                }
                Rectangle { Layout.fillWidth: true; Layout.minimumWidth: 0; height: 1; color: Theme.border }
                RowLayout {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    Repeater {
                        model: [
                            { value: "￥" + Number(appController.pricing.current_price_per_kwh || appController.selectedStation.price_per_kwh || 0).toFixed(2), label: "当前每度" },
                            { value: String(appController.selectedStation.pile_count || 0), label: "总桩" },
                            { value: String(appController.selectedStation.idle_pile_count || 0), label: "空闲" },
                            { value: (Number(appController.selectedStation.pile_count || 0) > 0 ? Math.round(100 * (Number(appController.selectedStation.pile_count) - Number(appController.selectedStation.offline_count || 0)) / Number(appController.selectedStation.pile_count)) : 0) + "%", label: "在线率" }
                        ]
                        delegate: Column {
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            spacing: 2
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.value; color: Theme.primaryDark; font.pixelSize: 18; font.bold: true }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: Theme.textMuted; font.pixelSize: 11 }
                        }
                    }
                }
            }
        }
        AppCard {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            implicitHeight: pricingContent.implicitHeight + 36
            ColumnLayout {
                id: pricingContent
                anchors.fill: parent
                anchors.margins: 18
                spacing: 6
                Text { text: "计价说明"; color: Theme.text; font.pixelSize: 15; font.bold: true }
                Repeater {
                    model: appController.pricing.periods || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        Text {
                            Layout.fillWidth: true
                            text: page.minuteRange(modelData.start_minute) + " - " + page.minuteRange(modelData.end_minute)
                            color: Theme.text; font.pixelSize: 12
                        }
                        Text { text: page.periodLabel(modelData.period_type); color: Theme.textMuted; font.pixelSize: 12 }
                        Text { text: "￥" + Number(modelData.price_per_kwh).toFixed(2) + "/度"; color: Theme.primaryDark; font.bold: true; font.pixelSize: 12 }
                    }
                }
                Text {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    visible: !(appController.pricing.periods || []).length
                    text: "本站按固定电价计费，暂无分时时段"
                    color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap
                }
                Rectangle { Layout.fillWidth: true; Layout.minimumWidth: 0; height: 1; color: Theme.border }
                Text { Layout.fillWidth: true; Layout.minimumWidth: 0; text: page.ruleText; color: Theme.textMuted; font.pixelSize: 11; wrapMode: Text.WordWrap }
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            spacing: 10
            AppButton {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                text: "驾车导航"
                onClicked: { appController.openNavigation("drive"); page.openMap() }
            }
            AppButton {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                text: "步行导航"
                variant: "secondary"
                onClicked: { appController.openNavigation("walk"); page.openMap() }
            }
        }
        Text {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            text: "预约占用由订单唯一约束保证:每个用户/每个电桩同时仅支持一个活动订单,预约后请及时开始充电(超 15 分钟未开始将自动释放)。"
            color: Theme.textMuted; font.pixelSize: 11; wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            Text { text: "站内电桩"; color: Theme.text; font.pixelSize: 19; font.bold: true }
            Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
            Text { text: appController.piles.length + " 台"; color: Theme.textMuted; font.pixelSize: 12 }
        }
        Repeater {
            id: pileList
            model: appController.piles
            delegate: AppCard {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                implicitHeight: 96
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12
                    Rectangle {
                        width: 48; height: 48; radius: 15
                        color: modelData.status === "idle" ? Theme.primarySoft : Theme.backgroundSecondary
                        AppIcon { anchors.centerIn: parent; name: "bolt"; width: 24; height: 24; iconColor: modelData.status === "idle" ? Theme.primary : Theme.textMuted }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        spacing: 3
                        Text { text: modelData.code || ""; color: Theme.text; font.pixelSize: 16; font.bold: true }
                        Text { text: (modelData.type === "fast" ? "快充" : "慢充") + " · " + Number(modelData.power_kw || 0).toFixed(1) + " kW"; color: Theme.textMuted; font.pixelSize: 12 }
                    }
                    ColumnLayout {
                        spacing: 7
                        StatusBadge { Layout.alignment: Qt.AlignRight; status: modelData.status || "offline" }
                        AppButton {
                            visible: modelData.status === "idle"
                            implicitHeight: 34
                            text: appController.activeOrder.id ? "已有订单" : "预约"
                            enabled: !appController.busy && !appController.activeOrder.id
                            onClicked: appController.reserve(modelData.id, modelData.power_kw)
                        }
                    }
                }
            }
        }
    }
    }
}
