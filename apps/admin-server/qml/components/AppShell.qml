import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingAdmin
import Charging.UI
import "../pages"

Item {
    id: shell
    property int pageIndex: 0
    signal securitySettingsRequested()
    readonly property var titles: ["工作台","充电站管理","充电桩管理","订单管理","用户管理","智能预测","系统设置"]
    // 电站详情抽屉"去管理电桩":切到电桩管理页(index 2)并预筛选该站
    function openPileManagement(station) {
        shell.pageIndex = 2
        pilesPage.applyStationFocus(station)
    }
    Sidebar { id: sidebar; anchors.left:parent.left;anchors.top:parent.top;anchors.bottom:parent.bottom;currentIndex:shell.pageIndex;onSelected:function(i){shell.pageIndex=i};onSecuritySettingsRequested:shell.securitySettingsRequested() }
    TopBar { anchors.left:sidebar.right;anchors.right:parent.right;anchors.top:parent.top;pageTitle:shell.titles[shell.pageIndex] }
    StackLayout {
        anchors.left:sidebar.right;anchors.right:parent.right;anchors.top:parent.top;anchors.topMargin:64;anchors.bottom:parent.bottom
        currentIndex:shell.pageIndex
        DashboardPage{} StationsPage{ onManagePilesRequested: function(station) { shell.openPileManagement(station) } } PilesPage{ id: pilesPage } OrdersPage{} UsersPage{} PredictionPage{} SettingsPage{}
    }
}
