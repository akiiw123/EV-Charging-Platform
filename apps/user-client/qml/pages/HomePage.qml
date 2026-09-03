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
                AppIcon { anchors.centerIn: parent; name: "bolt"; iconColor: Theme.primary; width: 24; height: 24 }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: search
                Layout.fillWidth: true
                implicitHeight: 48
                placeholderText: appController.mapKeyConfigured ? "输入任意地址定位(地图Key已配置)" : "输入城市名定位(如:深圳;配置地图Key后支持任意地址)"
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
        // 城市快选(模拟 GPS/区域选择):内置坐标,离线可用
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: appController.presetCities()
                delegate: Rectangle {
                    required property var modelData
                    radius: 12
                    implicitWidth: cityText.implicitWidth + 22
                    implicitHeight: 30
                    color: appController.locationName.indexOf(modelData) === 0
                           ? Theme.primarySoft : Theme.surface
                    border.width: 1
                    border.color: appController.locationName.indexOf(modelData) === 0
                                  ? Theme.primary : Theme.border
                    Text {
                        id: cityText
                        anchors.centerIn: parent
                        text: modelData
                        color: appController.locationName.indexOf(modelData) === 0
                               ? Theme.primaryDark : Theme.textMuted
                        font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: appController.locate(modelData)
                    }
                }
            }
            Item { Layout.fillWidth: true }
            Text {
                text: "当前: " + appController.locationName
                      + " (" + Number(appController.latitude).toFixed(2) + ", "
                      + Number(appController.longitude).toFixed(2) + ")"
                color: Theme.textMuted
                font.pixelSize: 11
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
