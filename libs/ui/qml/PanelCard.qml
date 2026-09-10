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
