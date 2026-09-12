import QtQuick
import QtQuick.Layouts
import ChargingUser

AppCard {
    id: card
    property var station
    signal opened()
    implicitHeight: 156

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 7
        RowLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            Text {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                text: station.name || ""
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                text: Number(station.distance_km || 0).toFixed(1) + " km"
                color: Theme.primary
                font.pixelSize: 14
                font.bold: true
            }
        }
        Text {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            text: station.address || ""
            color: Theme.textMuted
            font.pixelSize: 13
            elide: Text.ElideRight
        }
        Rectangle { Layout.fillWidth: true; Layout.minimumWidth: 0; height: 1; color: Theme.border }
        RowLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            spacing: 10
            Text {
                text: "￥" + Number(station.price_per_kwh || 0).toFixed(2)
                color: Theme.primaryDark
                font.pixelSize: 22
                font.bold: true
            }
            Text { text: "/度"; color: Theme.textMuted; font.pixelSize: 12 }
            Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
            Rectangle {
                implicitWidth: availability.implicitWidth + 18
                implicitHeight: 30
                radius: 15
                color: Number(station.idle_pile_count) > 0 ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.14) : Qt.rgba(Theme.textMuted.r, Theme.textMuted.g, Theme.textMuted.b, 0.10)
                Text {
                    id: availability
                    anchors.centerIn: parent
                    text: "空闲 " + (station.idle_pile_count || 0) + "/" + (station.pile_count || 0)
                    color: Number(station.idle_pile_count) > 0 ? Theme.success : Theme.textMuted
                    font.pixelSize: 12
                    font.bold: true
                }
            }
            Text { text: "›"; color: Theme.textMuted; font.pixelSize: 28 }
        }
    }
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: card.opened()
    }
}
