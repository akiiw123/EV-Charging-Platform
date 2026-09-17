/*
 * 文件职责：空状态组件：在无数据、加载失败等场景提供统一图标、说明和操作入口。
 * 对接关系：属于 UserAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import ChargingUser

// 统一的空状态:圆形图标 + 标题 + 提示,在可用宽度内水平居中、垂直居中
Item {
    id: empty
    property string icon: "order"
    property string title: "暂无内容"
    property string hint: ""
    implicitHeight: 176

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        Rectangle {
            width: 64; height: 64; radius: 32
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.primarySoft
            AppIcon { anchors.centerIn: parent; name: empty.icon; iconColor: Theme.primaryDark; width: 30; height: 30 }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: empty.title
            color: Theme.text
            font.pixelSize: 14
            font.bold: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: empty.hint.length > 0
            text: empty.hint
            color: Theme.textMuted
            font.pixelSize: 11
        }
    }
}
