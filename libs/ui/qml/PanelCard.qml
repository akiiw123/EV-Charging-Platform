/*
 * 文件职责：共享面板容器：提供统一表面、边框、圆角和内容插槽。
 * 对接关系：属于 Charging.UI 共享设计系统；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick

Rectangle {
    id: card
    property bool interactive: false
    color: interactive && mouse.containsMouse ? Theme.surfaceHover : Theme.surface
    radius: Theme.radiusMedium
    border.width: 1
    border.color: interactive && mouse.containsMouse ? Theme.borderStrong : Theme.borderSubtle
    layer.enabled: false
    Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: card.interactive; acceptedButtons: Qt.NoButton }
}
