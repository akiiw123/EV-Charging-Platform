/*
 * 文件职责：电站摘要卡片：展示距离、状态和价格等信息，并发出查看详情事件。
 * 对接关系：属于 UserAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import QtQuick.Layouts
import ChargingUser

// 平铺列表行:名称为主要层级,价格/空闲为次要信息,行间以细分割线区分
Item {
    id: row
    property var station
    signal opened()

    implicitHeight: 96

    ColumnLayout {
        anchors.fill: parent
        spacing: 5

        RowLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            Text {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                text: station.name || ""
                color: Theme.text
                font.pixelSize: 15
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                text: Number(station.distance_km || 0).toFixed(1) + " km"
                color: Theme.textMuted
                font.pixelSize: 12
            }
        }
        Text {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            text: station.address || ""
            color: Theme.textMuted
            font.pixelSize: 12
            elide: Text.ElideRight
        }
        RowLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            Text {
                text: "￥" + Number(station.price_per_kwh || 0).toFixed(2) + " / 度"
                color: Theme.text
                font.pixelSize: 13
            }
            Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
            Text {
                text: "空闲 " + (station.idle_pile_count || 0) + "/" + (station.pile_count || 0)
                color: Number(station.idle_pile_count) > 0 ? Theme.success : Theme.textMuted
                font.pixelSize: 12
                font.bold: Number(station.idle_pile_count) > 0
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.minimumWidth: 0; height: 1; color: Theme.border }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: row.opened()
    }
}
