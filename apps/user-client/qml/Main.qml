import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingUser
import "components"
import "pages"

ApplicationWindow {
    id: app
    // 窗口几何由 main.cpp 在映射前按屏幕可用区域设定(首选 440x820,小屏自动缩小并居中);
    // 这里保持 visible:false,由 C++ 端 show(),避免映射后二次 resize 导致首帧不渲染
    width: 440
    height: 820
    visible: false
    title: "VoltFlow 智充管理平台"
    color: Theme.background
    font.family: Theme.fontFamily

    // 内容列最大宽度:手机尺寸优先,宽窗口下居中显示、两侧留背景,避免组件被硬拉伸
    readonly property real contentWidth: Math.min(width, 480)

    Binding { target: Theme; property: "currentTheme"; value: appController.theme }

    property string currentPage: "home"
    property int currentTab: currentPage === "home" ? 0
                         : currentPage === "charging" ? 1
                         : currentPage === "orders" ? 2
                         : 3
    // 未完成订单引导弹窗:每次登录只弹一次,登出后重置
    property bool orderPromptShown: false

    // 活动订单状态的中文描述,供引导弹窗展示
    function orderStatusText(status) {
        return appController.orderStatusText(status)
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
                color: Theme.surface
                // 与管理端一致的轻顶栏:白色表面 + 底部细分隔线 + 深色文字
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border }
                RowLayout {
                    width: Math.min(parent.width, app.contentWidth)
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    height: 64
                Image { Layout.preferredWidth: 26; Layout.preferredHeight: 26; source: "qrc:/ChargingUser/assets/voltflow-logo.png"; fillMode: Image.PreserveAspectFit }
                Text { text: "VoltFlow 智充管理平台"; color: Theme.text; font.pixelSize: 16; font.bold: true }
                Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                Column {
                    Layout.preferredWidth: Math.min(120, app.width * 0.28)
                    clip: true
                    Text {
                        anchors.right: parent.right
                        width: parent.width
                        horizontalAlignment: Text.AlignRight
                        text: appController.user.nickname || "用户"
                        color: Theme.text
                        font.pixelSize: 13
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        anchors.right: parent.right
                        text: "￥" + Number(appController.user.wallet_balance || 0).toFixed(2)
                        color: Theme.textMuted
                        font.pixelSize: 11
                    }
                }
            }
        }

        Loader {
            id: pageLoader
            anchors.top: topBar.bottom
            anchors.bottom: bottomNav.visible ? bottomNav.top : parent.bottom
            width: Math.min(parent.width, app.contentWidth)
            anchors.horizontalCenter: parent.horizontalCenter
            active: appController.loggedIn
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
        height: noticeText.implicitHeight + 28
        visible: appController.notice.length > 0
        radius: 6
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
            width: parent.width - 32
            wrapMode: Text.Wrap
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
            if (!appController.loggedIn) { app.orderPromptShown = false; activeOrderDialog.close(); app.currentPage = "home" }
        }
    }

    Dialog {
        id: activeOrderDialog
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: parent
        width: 320
        padding: 20
        background: Rectangle { radius: 8; color: Theme.surface }
        contentItem: ColumnLayout {
            spacing: 14
            Text {
                text: "未完成订单提醒"
                font.pixelSize: 18
                font.bold: true
                color: Theme.text
            }
            Text {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                wrapMode: Text.WordWrap
                font.pixelSize: 14
                color: Theme.textMuted
                text: appController.activeOrder.id
                      ? "检测到您有一笔" + app.orderStatusText(appController.activeOrder.status)
                        + "的订单(单号 " + appController.activeOrder.id + "),请先处理后再发起新的预约。"
                      : "检测到您有未完成的充电订单,请先处理后再发起新的预约。"
            }
            AppButton {
                Layout.fillWidth: true; Layout.minimumWidth: 0
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
