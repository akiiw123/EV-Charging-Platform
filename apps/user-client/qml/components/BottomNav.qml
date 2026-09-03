import QtQuick
import QtQuick.Layouts
import ChargingUser

Rectangle {
    id: nav
    property int currentIndex: 0
    signal selected(int index)
    height: 76
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 8
        Repeater {
            model: [
                { icon: "home", text: "首页" },
                { icon: "bolt", text: "充电" },
                { icon: "person", text: "我的" }
            ]
            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 7
                radius: 14
                color: nav.currentIndex === index ? Theme.primarySoft : "transparent"
                Column {
                    anchors.centerIn: parent
                    spacing: 3
                    AppIcon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        name: modelData.icon
                        iconColor: nav.currentIndex === index ? Theme.primary : Theme.textMuted
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.text
                        font.pixelSize: 12
                        font.bold: nav.currentIndex === index
                        color: nav.currentIndex === index ? Theme.primaryDark : Theme.textMuted
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: nav.selected(index)
                }
                Behavior on color { ColorAnimation { duration: 160 } }
            }
        }
    }
}
