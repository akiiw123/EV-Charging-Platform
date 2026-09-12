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
    AppShell { anchors.fill: parent; visible: adminController.loggedIn
        onSecuritySettingsRequested: changePasswordDialog.openManual() }

    // 改密弹窗:首登检测到初始密码时强制提醒(每次登录都会再提醒);
    // 也可随时通过侧边栏"安全设置"手动打开。可见性由 openForced/openManual/closeSelf 显式驱动。
    Dialog {
        id: changePasswordDialog
        property bool manual: false
        property bool forcedDismissed: false
        modal: true
        closePolicy: Popup.CloseOnEscape
        visible: false
        anchors.centerIn: parent
        width: 430
        padding: 24
        function resetFields() { oldPwd.text = ""; newPwd.text = ""; confirmPwd.text = ""; cpError.text = "" }
        function openForced() { if (forcedDismissed) return; manual = false; resetFields(); visible = true }
        function openManual() { manual = true; resetFields(); visible = true }
        function closeSelf() { visible = false; manual = false }
        onRejected: closeSelf()
        background: PanelCard {}
        contentItem: ColumnLayout {
            spacing: 10
            Text { text: changePasswordDialog.manual ? "安全设置 · 修改密码" : "请修改初始密码"; color: Theme.textPrimary; font.pixelSize: Theme.fontTitle; font.bold: true }
            Text {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: changePasswordDialog.manual
                      ? "定期更换管理员密码可以降低账号泄露风险。修改成功后需要使用新密码重新登录。"
                      : "检测到当前账号仍在使用初始密码。为保障运营数据安全，请设置新密码后继续使用控制台。"
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
                    text: "取消"
                    variant: "secondary"
                    onClicked: {
                        // 手动打开:直接关闭;首登提醒:本次登录内不再打扰
                        if (changePasswordDialog.manual) changePasswordDialog.manual = false
                        else changePasswordDialog.forcedDismissed = true
                        changePasswordDialog.closeSelf()
                    }
                }
                AppButton {
                    Layout.fillWidth: true
                    enabled: !adminController.busy
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
                // 改密成功(或取消/退出登录)时收起弹窗;失败时展示服务端原因
                function onPasswordChangeResult(success) {
                    if (success) changePasswordDialog.closeSelf()
                    else cpError.text = adminController.errorMessage
                }
                function onLoggedInChanged() {
                    if (adminController.loggedIn) {
                        // 每次登录都重新检查初始密码状态(每个登录周期最多提醒一次)
                        changePasswordDialog.forcedDismissed = false
                        if (adminController.mustChangePassword) changePasswordDialog.openForced()
                    } else {
                        changePasswordDialog.forcedDismissed = false
                        changePasswordDialog.closeSelf()
                    }
                }
                function onMustChangePasswordChanged() {
                    if (adminController.loggedIn && adminController.mustChangePassword)
                        changePasswordDialog.openForced()
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

