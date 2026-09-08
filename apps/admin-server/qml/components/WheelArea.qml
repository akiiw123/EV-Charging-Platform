// 把鼠标滚轮和触控板增量转换为可控滚动距离。
import QtQuick

// 统一处理鼠标滚轮事件
// 可绑定到 ListView、Flickable、ScrollView.contentItem 等可滚动对象
MouseArea {
    property var flickable

    anchors.fill: parent
    acceptedButtons: Qt.NoButton

    onWheel: function(wheel) {
        // 未绑定滚动对象时，把滚轮事件继续向下传递
        if (!flickable) {
            wheel.accepted = false
            return
        }

        // Shift + 滚轮，或横向滚轮输入时，执行横向滚动
        var horizontal =
                (wheel.modifiers & Qt.ShiftModifier) !== 0
                || Math.abs(wheel.angleDelta.x) > Math.abs(wheel.angleDelta.y)

        var pixel = horizontal ? wheel.pixelDelta.x : wheel.pixelDelta.y
        var angle = horizontal ? wheel.angleDelta.x : wheel.angleDelta.y

        // 某些鼠标在横向模式下仍只提供 y 方向滚轮值
        if (horizontal && angle === 0)
            angle = wheel.angleDelta.y

        // 优先使用高精度 pixelDelta，没有时使用传统 angleDelta
        var delta = pixel !== 0 ? pixel : angle / 120 * 96

        var start = horizontal ? flickable.originX : flickable.originY
        var extent = horizontal
                ? flickable.contentWidth - flickable.width
                : flickable.contentHeight - flickable.height

        var before = horizontal
                ? flickable.contentX
                : flickable.contentY

        var after = Math.max(
                    start,
                    Math.min(
                        start + Math.max(0, extent),
                        before - delta
                    )
                )

        if (horizontal)
            flickable.contentX = after
        else
            flickable.contentY = after

        // 只有实际发生滚动时才消费事件
        wheel.accepted = after !== before
    }
}
