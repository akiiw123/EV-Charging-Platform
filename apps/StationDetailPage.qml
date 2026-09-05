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

    Connections {
        target: appController
        function onReservationSucceeded() { page.openCharging() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                width: 42; height: 42; radius: 13; color: Theme.surface
                border.width: 1; border.color: Theme.border
                Text { anchors.centerIn: parent; text: "‹"; font.pixelSize: 30; color: Theme.text }
                MouseArea { anchors.fill: parent; onClicked: page.back(); cursorShape: Qt.PointingHandCursor }
            }
            Text {
                Layout.fillWidth: true
                text: "电站详情"
                color: Theme.text
                font.pixelSize: 20
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }
            Item { width: 42; height: 42 }
        }
        AppCard {
            Layout.fillWidth: true
            implicitHeight: 186
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 7
                Text { Layout.fillWidth: true; text: appController.selectedStation.name || ""; color: Theme.text; font.pixelSize: 22; font.bold: true; wrapMode: Text.Wrap }
                Text { Layout.fillWidth: true; text: appController.selectedStation.address || ""; color: Theme.textMuted; font.pixelSize: 13; wrapMode: Text.Wrap }
                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
                RowLayout {
                    Layout.fillWidth: true
                    Repeater {
                        model: [
                            { value: "￥" + Number(appController.selectedStation.price_per_kwh || 0).toFixed(2), label: "每度" },
                            { value: String(appController.selectedStation.pile_count || 0), label: "总桩" },
                            { value: String(appController.selectedStation.idle_pile_count || 0), label: "空闲" },
                            { value: (Number(appController.selectedStation.pile_count || 0) > 0 ? Math.round(100 * (Number(appController.selectedStation.pile_count) - Number(appController.selectedStation.offline_count || 0)) / Number(appController.selectedStation.pile_count)) : 0) + "%", label: "在线率" }
                        ]
                        delegate: Column {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.value; color: Theme.primaryDark; font.pixelSize: 18; font.bold: true }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: Theme.textMuted; font.pixelSize: 11 }
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            AppButton {
                Layout.fillWidth: true
                text: "驾车导航"
                onClicked: { appController.openNavigation("drive"); page.openMap() }
            }
            AppButton {
                Layout.fillWidth: true
                text: "步行导航"
                variant: "secondary"
                onClicked: { appController.openNavigation("walk"); page.openMap() }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Text { text: "站内电桩"; color: Theme.text; font.pixelSize: 19; font.bold: true }
            Item { Layout.fillWidth: true }
            Text { text: appController.piles.length + " 台"; color: Theme.textMuted; font.pixelSize: 12 }
        }
        ListView {
            id: pileList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: appController.piles
            delegate: AppCard {
                width: pileList.width
                implicitHeight: 96
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12
                    Rectangle {
                        width: 48; height: 48; radius: 15
                        color: modelData.status === "idle" ? Theme.primarySoft : "#F1F5F9"
                        AppIcon { anchors.centerIn: parent; name: "bolt"; width: 24; height: 24; iconColor: modelData.status === "idle" ? Theme.primary : Theme.textMuted }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
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
