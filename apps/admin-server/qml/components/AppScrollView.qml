import QtQuick
import QtQuick.Controls

// 管理端统一滚动容器
// 页面内容超出可视区域时提供纵向滚动能力
ScrollView {
    id: root

    clip: true
    contentWidth: availableWidth

    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: ScrollBar.AsNeeded
}
