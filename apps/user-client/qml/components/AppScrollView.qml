// 统一滚轮和触控板滚动行为的滚动容器。
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
