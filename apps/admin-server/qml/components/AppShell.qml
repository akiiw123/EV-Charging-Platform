import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChargingAdmin
import Charging.UI
import "../pages"

Item {
    id: shell
    property int pageIndex: 0
    readonly property var titles: ["数据总览","电站管理","电桩管理","订单管理","用户管理","智能预测","主题与设置"]
    Sidebar { id: sidebar; anchors.left:parent.left;anchors.top:parent.top;anchors.bottom:parent.bottom;currentIndex:shell.pageIndex;onSelected:function(i){shell.pageIndex=i} }
    TopBar { anchors.left:sidebar.right;anchors.right:parent.right;anchors.top:parent.top;pageTitle:shell.titles[shell.pageIndex] }
    StackLayout {
        anchors.left:sidebar.right;anchors.right:parent.right;anchors.top:parent.top;anchors.topMargin:64;anchors.bottom:parent.bottom
        currentIndex:shell.pageIndex
        DashboardPage{} StationsPage{} PilesPage{} OrdersPage{} UsersPage{} PredictionPage{} SettingsPage{}
    }
}
