import QtQuick
import QtQuick.Controls
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

