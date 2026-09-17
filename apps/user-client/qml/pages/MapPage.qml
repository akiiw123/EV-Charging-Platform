/*
 * 文件职责：地图导航页：承载腾讯地图 Web 页面；地图 Key 和 URL 由 C++ Controller 统一准备。
 * 对接关系：属于 UserAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
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
            Layout.fillWidth: true; Layout.minimumWidth: 0
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
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    text: appController.mapTitle || "地图导航"
                    color: Theme.text
                    font.pixelSize: 17
                    font.bold: true
                    elide: Text.ElideRight
                }
            }
        }
        WebEngineView {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            Layout.fillHeight: true
            url: appController.mapUrl
        }
    }
}
