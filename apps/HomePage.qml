import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser
import "../components"

Item {
    id: page
    signal openStation()
    readonly property var applied: appController.filters
    readonly property int filterCount: (applied.minDistance > 0 || applied.maxDistance >= 0 ? 1 : 0)
        + (applied.minPrice > 0 || applied.maxPrice >= 0 ? 1 : 0)
        + (applied.type !== "" ? 1 : 0) + (applied.idleOnly ? 1 : 0)


    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Column {
                Layout.fillWidth: true
                spacing: 2
                Text { text: "当前位置"; color: Theme.textMuted; font.pixelSize: 11 }
                Text { width: parent.width; elide: Text.ElideRight; text: appController.locationName; color: Theme.text; font.pixelSize: 20; font.bold: true }
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
            AppField {
                id: search
                Layout.fillWidth: true
                implicitHeight: 48
                placeholderText: "输入城市或地址定位"
                leftPadding: 16
                background: Rectangle {
                    radius: 14
                    color: Theme.surface
                    border.width: 1
                    border.color: search.activeFocus ? Theme.primary : Theme.border
                }

                onAccepted: appController.locate(text)
            }
            AppButton {
                text: appController.locating ? "定位中" : "定位"
                enabled: !appController.locating
                implicitWidth: 78
                onClicked: appController.locate(search.text)
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: appController.presetCities()
                delegate: AppButton {
                    required property var modelData
                    text: modelData
                    implicitWidth: 50
                    leftPadding: 4
                    rightPadding: 4
                    font.pixelSize: 12
                    variant: appController.locationName.indexOf(modelData) === 0 ? "primary" : "secondary"
                    implicitHeight: 32
                    enabled: !appController.locating
                    onClicked: appController.locate(modelData)
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            AppField {
                Layout.fillWidth: true
                placeholderText: "搜索电站名称或地址"
                text: appController.searchQuery
                onTextEdited: appController.searchQuery = text
            }

        }
        Button {
            id: filterEntry
            objectName: "filterEntry"
            Layout.fillWidth: true
            implicitHeight: 58
            padding: 12
            Accessible.name: "筛选电站，距离、价格、充电类型和空闲状态"
            onClicked: filters.open()
            background: Rectangle {
                radius: 14
                color: filterEntry.down ? "#D5E5FF" : Theme.primarySoft
                border.color: filterEntry.activeFocus || filterEntry.hovered ? Theme.primary : "#C8DCFF"
            }
            contentItem: RowLayout {
                spacing: 10
                Text { text: "筛选电站"; font.pixelSize: 15; font.bold: true; color: Theme.primaryDark }
                Text {
                    Layout.fillWidth: true
                    text: page.filterCount ? "已启用 " + page.filterCount + " 项条件" : "距离 / 价格 / 类型"
                    font.pixelSize: 12
                    color: Theme.primaryDark
                    elide: Text.ElideRight
                }
                Text { text: "展开 ›"; font.pixelSize: 13; font.bold: true; color: Theme.primaryDark }
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
    AppDialog {
        id: filters
        objectName: "filterDialog"
        property int typeIndex: 0
        acceptText: "应用筛选"
        anchors.centerIn: parent
        width: Math.min(page.width - 24, 360)
        height: Math.min(page.height - 16, 500)
        title: "组合筛选"
        modal: true
        onOpened: {
            var f = appController.filters
            minDistance.text = f.minDistance || ""
            maxDistance.text = f.maxDistance < 0 ? "" : f.maxDistance
            minPrice.text = f.minPrice || ""
            maxPrice.text = f.maxPrice < 0 ? "" : f.maxPrice
            filters.typeIndex = ["", "fast", "slow"].indexOf(f.type)
            idle.checked = f.idleOnly
        }
        onAccepted: appController.setFilters(Number(minDistance.text || 0), maxDistance.text === "" ? -1 : Number(maxDistance.text),
            Number(minPrice.text || 0), maxPrice.text === "" ? -1 : Number(maxPrice.text), ["", "fast", "slow"][filters.typeIndex], idle.checked)
        contentItem: ScrollView {
            id: filterScroll
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: filterScroll.availableWidth
                spacing: 12
                Text { text: "距离范围 · 公里"; color: Theme.text; font.bold: true; font.pixelSize: 14 }
                RowLayout {
                    Layout.fillWidth: true
                    AppField { id: minDistance; Layout.fillWidth: true; placeholderText: "最低 0"; validator: DoubleValidator { bottom: 0; locale: "C" } }
                    AppField { id: maxDistance; Layout.fillWidth: true; placeholderText: "最高不限"; validator: DoubleValidator { bottom: 0; locale: "C" } }
                }
                Text { text: "价格范围 · 元/度"; color: Theme.text; font.bold: true; font.pixelSize: 14 }
                RowLayout {
                    Layout.fillWidth: true
                    AppField { id: minPrice; Layout.fillWidth: true; placeholderText: "最低 0"; validator: DoubleValidator { bottom: 0; locale: "C" } }
                    AppField { id: maxPrice; Layout.fillWidth: true; placeholderText: "最高不限"; validator: DoubleValidator { bottom: 0; locale: "C" } }
                }
                Text { text: "充电类型"; color: Theme.text; font.bold: true; font.pixelSize: 14 }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: ["不限", "快充", "慢充"]
                        delegate: AppButton {
                            required property int index
                            required property string modelData
                            Layout.fillWidth: true
                            implicitHeight: 38
                            text: modelData
                            variant: filters.typeIndex === index ? "primary" : "secondary"
                            onClicked: filters.typeIndex = index
                        }
                    }
                }
                CheckBox {
                    id: idle
                    Layout.fillWidth: true
                    text: "仅看有空闲桩"
                    implicitHeight: 40
                    indicator: Rectangle {
                        x: 0; y: (idle.height - height) / 2
                        width: 22; height: 22; radius: 7
                        color: idle.checked ? Theme.primary : Theme.surface
                        border.color: idle.checked || idle.activeFocus ? Theme.primary : Theme.border
                        Text { anchors.centerIn: parent; text: idle.checked ? "✓" : ""; color: "white"; font.bold: true }
                    }
                    contentItem: Text { text: idle.text; leftPadding: 32; verticalAlignment: Text.AlignVCenter; color: Theme.text; font.pixelSize: 14 }
                }
                Text { Layout.fillWidth: true; text: "上限留空表示不限，多个条件同时生效。"; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: 12 }
                AppButton {
                    Layout.fillWidth: true
                    variant: "secondary"
                    implicitHeight: 38
                    text: "重置所有筛选"
                    onClicked: {
                        minDistance.clear(); maxDistance.clear(); minPrice.clear(); maxPrice.clear(); filters.typeIndex = 0; idle.checked = false
                        appController.searchQuery = ""
                        appController.setFilters(0, -1, 0, -1, "", false)
                    }
                }
            }
        }
    }
}
