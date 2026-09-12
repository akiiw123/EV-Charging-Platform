import QtQuick
import QtQuick.Layouts
import ChargingUser

Rectangle {
    id: nav
    property int currentIndex: 0
    signal selected(int index)
    height: 64
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    RowLayout {
        // 宽窗口下内容列同样限制在手机宽度内,与页面列对齐
        width: Math.min(parent.width, 480)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        height: parent.height - 2
        spacing: 8
        Repeater {
            model: [
                { icon: "home", text: "首页" },
                { icon: "bolt", text: "充电" },
                { icon: "order", text: "订单" },
                { icon: "person", text: "我的" }
            ]
            delegate: Item {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.fillHeight: true
                Column {
                    anchors.centerIn: parent
                    spacing: 3
                    AppIcon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 18; height: 18
                        name: modelData.icon
                        iconColor: nav.currentIndex === index ? Theme.primary : Theme.textMuted
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.text
                        font.pixelSize: 11
                        font.bold: nav.currentIndex === index
                        color: nav.currentIndex === index ? Theme.primary : Theme.textMuted
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: nav.selected(index)
                }
            }
        }
    }
}
