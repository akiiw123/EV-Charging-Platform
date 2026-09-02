import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser
import "../components"

Item {
    id: page
    signal openStation()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            Column {
                spacing: 2
                Text { text: "当前位置"; color: Theme.textMuted; font.pixelSize: 11 }
                Text { text: "⌖ " + appController.locationName; color: Theme.text; font.pixelSize: 20; font.bold: true }
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 44; height: 44; radius: 14
                color: Theme.primarySoft
                Text { anchors.centerIn: parent; text: "ϟ"; color: Theme.primary; font.pixelSize: 25; font.bold: true }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: search
                Layout.fillWidth: true
                implicitHeight: 48
                placeholderText: "搜索地址或充电站"
                leftPadding: 16
                background: Rectangle {
                    radius: 14
                    color: Theme.surface
                    border.width: 1
                    border.color: search.activeFocus ? Theme.primary : Theme.border
                }
                onTextChanged: appController.searchQuery = text
                onAccepted: appController.locate(text)
            }
            AppButton {
                text: "定位"
                implicitWidth: 78
                onClicked: appController.locate(search.text)
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Text { text: "附近充电站"; color: Theme.text; font.pixelSize: 20; font.bold: true }
            Item { Layout.fillWidth: true }
            Text { text: appController.stations.length + " 个站点"; color: Theme.textMuted; font.pixelSize: 12 }
        }
        ListView {
            id: stationList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 12
            model: appController.stations
            boundsBehavior: Flickable.OvershootBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: StationCard {
                width: stationList.width
                station: modelData
                onOpened: {
                    appController.selectStation(modelData)
                    page.openStation()
                }
            }
            footer: Item { width: 1; height: 8 }
        }
        Column {
            Layout.alignment: Qt.AlignCenter
            visible: appController.stations.length === 0
            spacing: 8
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "⌕"; font.pixelSize: 40; color: Theme.textMuted }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "没有找到匹配的充电站"; color: Theme.textMuted }
        }
    }
}
