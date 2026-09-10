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
