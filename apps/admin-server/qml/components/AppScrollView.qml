/*
 * 文件职责：滚动容器：统一滚动条与滚轮行为，供长页面复用。
 * 对接关系：属于 AdminAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import QtQuick.Controls

ScrollView {
    id: view
    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical.policy: ScrollBar.AsNeeded
    ScrollBar.vertical.minimumSize: 0.12
    WheelArea { parent: view; flickable: view.contentItem }
}
