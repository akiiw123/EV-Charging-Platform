/*
 * 文件职责：通用卡片容器：为页面内容提供一致的背景、边框、圆角和内边距。
 * 对接关系：属于 UserAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import ChargingUser

Rectangle {
    color: Theme.surface
    radius: Theme.radius
    border.width: 1
    border.color: Theme.border
}
