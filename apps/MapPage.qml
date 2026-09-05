import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine
import ChargingUser
import "../components"

Item {
    id: page
    signal back()
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 62
            color: Theme.surface
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 18
                Rectangle {
                    width: 42; height: 42; radius: 13; color: Theme.primarySoft
                    Text { anchors.centerIn: parent; text: "‹"; font.pixelSize: 30; color: Theme.primaryDark }
                    MouseArea { anchors.fill: parent; onClicked: page.back(); cursorShape: Qt.PointingHandCursor }
                }
                Text {
                    Layout.fillWidth: true
                    text: appController.mapTitle || "地图导航"
                    color: Theme.text
                    font.pixelSize: 17
                    font.bold: true
                    elide: Text.ElideRight
                }
            }
        }
        WebEngineView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            url: appController.mapUrl
        }
    }
}
