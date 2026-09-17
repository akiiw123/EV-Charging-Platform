/*
 * 文件职责：鼠标滚轮适配区域：把滚轮输入转交给需要的列表或选择控件。
 * 对接关系：属于 UserAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick

MouseArea {
    property var flickable
    anchors.fill: parent
    acceptedButtons: Qt.NoButton
    onWheel: function(wheel) {
        if (!flickable) { wheel.accepted = false; return }
        var horizontal = (wheel.modifiers & Qt.ShiftModifier) !== 0 || Math.abs(wheel.angleDelta.x) > Math.abs(wheel.angleDelta.y)
        var pixel = horizontal ? wheel.pixelDelta.x : wheel.pixelDelta.y
        var angle = horizontal ? wheel.angleDelta.x : wheel.angleDelta.y
        if (horizontal && angle === 0) angle = wheel.angleDelta.y
        var delta = pixel !== 0 ? pixel : angle / 120 * 96
        var start = horizontal ? flickable.originX : flickable.originY
        var extent = horizontal ? flickable.contentWidth - flickable.width : flickable.contentHeight - flickable.height
        var before = horizontal ? flickable.contentX : flickable.contentY
        var after = Math.max(start, Math.min(start + Math.max(0, extent), before - delta))
        if (horizontal) flickable.contentX = after
        else flickable.contentY = after
        wheel.accepted = after !== before
    }
}
