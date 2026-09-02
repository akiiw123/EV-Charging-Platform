import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser
import "components"
import "pages"

ApplicationWindow {
    id: app
    width: 440
    height: 820
    minimumWidth: 390
    minimumHeight: 680
    visible: true
    title: "充电客户端"
    color: Theme.background

    property string currentPage: "home"
    property int currentTab: currentPage === "home" ? 0
                             : currentPage === "charging" ? 1 : 2

    function showHome() { currentPage = "home" }
    function showCharging() { currentPage = "charging" }
    function showProfile() { currentPage = "profile"; appController.refreshProfile() }
    function showStation() { currentPage = "station" }
    function showMap() { currentPage = "map" }

    Component {
        id: loginComponent
        LoginPage { onCompleted: app.showHome() }
    }
    Component {
        id: homeComponent
        HomePage { onOpenStation: app.showStation() }
    }
    Component {
        id: stationComponent
        StationDetailPage {
            onBack: app.showHome()
            onOpenMap: app.showMap()
            onOpenCharging: app.showCharging()
        }
    }
    Component { id: chargingComponent; ChargingPage {} }
    Component {
        id: profileComponent
        ProfilePage { onLoggedOut: app.showHome() }
    }
    Component {
        id: mapComponent
        MapPage { onBack: app.showStation() }
    }

    Loader {
        id: loginLoader
        anchors.fill: parent
        active: !appController.loggedIn
        sourceComponent: loginComponent
        visible: active
    }

    Item {
        id: shell
        anchors.fill: parent
        visible: appController.loggedIn

        Rectangle {
            id: topBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: app.currentPage === "map" ? 0 : 64
            visible: height > 0
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: Theme.primaryDark }
                GradientStop { position: 1; color: Theme.primary }
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                Rectangle {
                    width: 38; height: 38; radius: 13; color: "#24FFFFFF"
                    Text { anchors.centerIn: parent; text: "ϟ"; color: "white"; font.pixelSize: 25; font.bold: true }
                }
                Text { text: "充电客户端"; color: "white"; font.pixelSize: 18; font.bold: true }
                Item { Layout.fillWidth: true }
                Column {
                    Layout.maximumWidth: 155
                    Text {
                        anchors.right: parent.right
                        text: appController.user.nickname || "用户"
                        color: "white"
                        font.pixelSize: 13
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        anchors.right: parent.right
                        text: "￥" + Number(appController.user.wallet_balance || 0).toFixed(2)
                        color: "#D9FFFFFF"
                        font.pixelSize: 11
                    }
                }
            }
        }

        Loader {
            id: pageLoader
            anchors.top: topBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: bottomNav.visible ? bottomNav.top : parent.bottom
            sourceComponent: app.currentPage === "home" ? homeComponent
                           : app.currentPage === "station" ? stationComponent
                           : app.currentPage === "charging" ? chargingComponent
                           : app.currentPage === "profile" ? profileComponent
                           : mapComponent
        }

        BottomNav {
            id: bottomNav
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            visible: app.currentPage === "home"
                     || app.currentPage === "charging"
                     || app.currentPage === "profile"
            currentIndex: app.currentTab
            onSelected: function(index) {
                if (index === 0) app.showHome()
                else if (index === 1) app.showCharging()
                else app.showProfile()
            }
        }
    }

    Rectangle {
        id: notice
        anchors.horizontalCenter: parent.horizontalCenter
        y: appController.notice.length > 0 ? 18 : -80
        width: Math.min(parent.width - 32, noticeText.implicitWidth + 46)
        height: 48
        radius: 14
        z: 100
        color: appController.noticeKind === "error" ? "#FFF0F0"
               : appController.noticeKind === "warning" ? "#FFF7E8"
               : "#E9FAF5"
        border.width: 1
        border.color: appController.noticeKind === "error" ? "#F5B8BA"
                      : appController.noticeKind === "warning" ? "#F6D28B"
                      : "#A8E5D4"
        Text {
            id: noticeText
            anchors.centerIn: parent
            text: appController.notice
            color: appController.noticeKind === "error" ? Theme.danger
                   : appController.noticeKind === "warning" ? "#A15C00"
                   : Theme.primaryDark
            font.pixelSize: 13
            font.bold: true
        }
        Behavior on y { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        MouseArea { anchors.fill: parent; onClicked: appController.clearNotice() }
    }

    Rectangle {
        anchors.fill: parent
        visible: appController.busy
        z: 90
        color: "#33000000"
        BusyIndicator {
            anchors.centerIn: parent
            running: parent.visible
        }
    }
}
