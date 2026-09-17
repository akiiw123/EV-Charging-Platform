/*
 * 文件职责：通用弹窗：统一标题、内容区、操作区和关闭行为。
 * 对接关系：属于 UserAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser

Dialog {
    id: dialog
    property string acceptText: "确认"
    property string cancelText: "取消"
    property bool acceptEnabled: true
    property bool showReset: false
    signal resetRequested()
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
            AppButton { visible: dialog.showReset; Layout.fillWidth: true; Layout.minimumWidth: 0; text: "重置"; variant: "secondary"; leftPadding: 8; rightPadding: 8; onClicked: dialog.resetRequested() }
            AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: dialog.cancelText; variant: "secondary"; onClicked: dialog.reject() }
            AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: dialog.acceptText; enabled: dialog.acceptEnabled; onClicked: dialog.accept() }
        }
    }
    Overlay.modal: Rectangle { color: "#660B1531" }
}
