import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser

Dialog {
    id: dialog
    property string acceptText: "确认"
    property string cancelText: "取消"
    property bool acceptEnabled: true
    modal: true
    padding: 20
    topPadding: 12
    bottomPadding: 12
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { radius: 20; color: Theme.surface; border.color: Theme.border }
    header: Label {
        text: dialog.title
        color: Theme.text
        font.pixelSize: 20
        font.bold: true
        padding: 20
        bottomPadding: 8
        wrapMode: Text.Wrap
    }
    footer: Item {
        implicitHeight: 76
        RowLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10
            AppButton { Layout.fillWidth: true; text: dialog.cancelText; variant: "secondary"; onClicked: dialog.reject() }
            AppButton { Layout.fillWidth: true; text: dialog.acceptText; enabled: dialog.acceptEnabled; onClicked: dialog.accept() }
        }
    }
    Overlay.modal: Rectangle { color: "#660B1531" }
}
