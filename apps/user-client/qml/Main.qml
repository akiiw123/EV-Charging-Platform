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
    font.family: Theme.fontFamily

    property string currentPage: "home"
    property int currentTab: currentPage === "home" ? 0
                         : currentPage === "charging" ? 1
                         : currentPage === "orders" ? 2
                         : 3
    // 未完成订单引导弹窗:每次登录只弹一次,登出后重置
    property bool orderPromptShown: false

    // 活动订单状态的中文描述,供引导弹窗展示
    function orderStatusText(status) {
        if (status === "charging") return "充电中"
        if (status === "awaiting_payment") return "待结算"
        return "预约待开始"
    }

    function showHome() { currentPage = "home" }
    function showCharging() { currentPage = "charging" }
    function showOrders() { currentPage = "orders" }
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
    id: ordersComponent
    OrdersPage {}
    }
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
                color: Theme.primaryDark
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    Rectangle {
                        width: 38; height: 38; radius: 12; color: "#24FFFFFF"
                        AppIcon { anchors.centerIn: parent; name: "bolt"; iconColor: "white"; width: 22; height: 22 }
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
               : app.currentPage === "orders" ? ordersComponent
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
         || app.currentPage === "orders"
         || app.currentPage === "profile"

        currentIndex: app.currentTab

        onSelected: function(index) {
            if (index === 0)
            app.showHome()
            else if (index === 1)
            app.showCharging()
            else if (index === 2)
            app.showOrders()
            else if (index === 3)
            app.showProfile()
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

    // 登录后检测到活动订单时弹窗提醒,并引导用户进入订单页处理
    // (考察点:QML 属性变化信号 onActiveOrderChanged 驱动界面状态)
    Connections {
        target: appController
        function onActiveOrderChanged() {
            if (!appController.loggedIn) return
            if (Object.keys(appController.activeOrder).length === 0) return
            if (app.orderPromptShown) return
            app.orderPromptShown = true
            activeOrderDialog.open()
        }
        function onLoggedInChanged() {
            if (!appController.loggedIn) app.orderPromptShown = false
        }
    }

    Dialog {
        id: activeOrderDialog
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: parent
        width: 320
        padding: 20
        background: Rectangle { radius: 16; color: Theme.surface }
        contentItem: ColumnLayout {
            spacing: 14
            Text {
                text: "未完成订单提醒"
                font.pixelSize: 18
                font.bold: true
                color: Theme.text
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 14
                color: Theme.textMuted
                text: appController.activeOrder.id
                      ? "检测到您有一笔" + app.orderStatusText(appController.activeOrder.status)
                        + "的订单(单号 " + appController.activeOrder.id + "),请先处理后再发起新的预约。"
                      : "检测到您有未完成的充电订单,请先处理后再发起新的预约。"
            }
            AppButton {
                Layout.fillWidth: true
                Layout.topMargin: 6
                text: "去处理"
                onClicked: {
                    activeOrderDialog.close()
                    app.showCharging()
                }
            }
        }
    }
}
