import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingAdmin
import Charging.UI
import "components"
import "pages"

ApplicationWindow {
    id: window
    width: 1440; height: 900; minimumWidth: 1280; minimumHeight: 720
    visible: true
    title: "电动汽车充电桩应用管理平台"
    color: Theme.backgroundPrimary
    Component.onCompleted: { Theme.currentTheme=adminController.theme; Theme.animationsEnabled=adminController.animationsEnabled; Theme.fontScale=adminController.fontScale }
    Connections { target: adminController
        function onThemeChanged(){Theme.currentTheme=adminController.theme}
        function onSettingsChanged(){Theme.animationsEnabled=adminController.animationsEnabled;Theme.fontScale=adminController.fontScale}
    }
    LoginPage { anchors.fill: parent; visible: !adminController.loggedIn }
    AppShell { anchors.fill: parent; visible: adminController.loggedIn }

    // 首登改密提醒:检测到初始密码时弹出,可关闭稍后处理(每次登录会再提醒)
    Dialog {
        id: changePasswordDialog
        modal: true
        closePolicy: Popup.CloseOnEscape
        visible: adminController.loggedIn && adminController.mustChangePassword
        anchors.centerIn: parent
        width: 430
        padding: 24
        background: PanelCard {}
        contentItem: ColumnLayout {
            spacing: 10
            Text { text: "请修改初始密码"; color: Theme.textPrimary; font.pixelSize: Theme.fontTitle; font.bold: true }
            Text {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: "检测到当前账号仍在使用初始密码。为保障运营数据安全，请设置新密码后继续使用控制台。"
                color: Theme.textSecondary; font.pixelSize: Theme.fontBody
            }
            Text { text: "当前密码"; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
            AppTextField { id: oldPwd; Layout.fillWidth: true; echoMode: TextInput.Password }
            Text { text: "新密码（至少 8 位）"; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
            AppTextField { id: newPwd; Layout.fillWidth: true; echoMode: TextInput.Password }
            Text { text: "确认新密码"; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
            AppTextField { id: confirmPwd; Layout.fillWidth: true; echoMode: TextInput.Password }
            Text { id: cpError; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.danger; font.pixelSize: Theme.fontCaption; visible: text.length > 0 }
            RowLayout {
                Layout.fillWidth: true; Layout.topMargin: 6
                AppButton {
                    Layout.fillWidth: true
                    text: "稍后再说"
                    variant: "secondary"
                    onClicked: changePasswordDialog.close()
                }
                AppButton {
                    Layout.fillWidth: true
                    text: "确认修改"
                    onClicked: {
                        cpError.text = ""
                        if (oldPwd.text.length === 0) { cpError.text = "请输入当前密码"; return }
                        if (newPwd.text.length < 8) { cpError.text = "新密码至少需要 8 位"; return }
                        if (newPwd.text !== confirmPwd.text) { cpError.text = "两次输入的新密码不一致"; return }
                        if (newPwd.text === oldPwd.text) { cpError.text = "新密码不能与当前密码相同"; return }
                        adminController.changePassword(oldPwd.text, newPwd.text)
                    }
                }
            }
            Connections {
                target: adminController
                // 改密成功后 mustChangePassword 变 false,弹窗随之关闭
                function onPasswordChangeResult(success) {
                    if (!success) cpError.text = adminController.errorMessage
                }
                function onMustChangePasswordChanged() {
                    if (adminController.mustChangePassword) {
                        oldPwd.text = ""; newPwd.text = ""; confirmPwd.text = ""; cpError.text = ""
                    }
                }
            }
        }
    }

    Rectangle {
        id: toast; z: 1000; width: Math.min(520,toastText.implicitWidth+48); height: 48; radius: Theme.radiusSmall
        anchors.horizontalCenter: parent.horizontalCenter; y: adminController.notice.length?20:-60
        color: adminController.noticeKind==="error"?Qt.rgba(Theme.danger.r,Theme.danger.g,Theme.danger.b,.16):Qt.rgba(Theme.success.r,Theme.success.g,Theme.success.b,.16)
        border.color: adminController.noticeKind==="error"?Theme.danger:Theme.success
        Text{id:toastText;anchors.centerIn:parent;text:adminController.notice;color:Theme.textPrimary;font.pixelSize:Theme.fontBody}
        Behavior on y{NumberAnimation{duration:Theme.durationNormal;easing.type:Easing.OutCubic}}
        MouseArea{anchors.fill:parent;onClicked:adminController.clearNotice()}
    }
}

