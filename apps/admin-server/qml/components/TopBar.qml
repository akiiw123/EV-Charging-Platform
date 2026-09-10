import QtQuick
import QtQuick.Layouts
import Charging.UI

Rectangle {
    property string pageTitle
    height:64;color:Theme.backgroundPrimary;border.color:Theme.borderSubtle
    RowLayout{anchors.fill:parent;anchors.leftMargin:24;anchors.rightMargin:24;spacing:16
        Rectangle{width:34;height:34;radius:8;color:"transparent";border.color:Theme.borderSubtle
            LineIcon{anchors.centerIn:parent;name:"menu"}
            MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:adminController.sidebarExpanded=!adminController.sidebarExpanded}}
        Text{text:pageTitle;color:Theme.textPrimary;font.pixelSize:Theme.fontTitle;font.weight:Font.DemiBold}
        Item{Layout.fillWidth:true}
        Row{spacing:8;Rectangle{width:8;height:8;radius:4;color:adminController.connected?Theme.success:Theme.danger;anchors.verticalCenter:parent.verticalCenter}Text{text:"服务";color:Theme.textSecondary;font.pixelSize:Theme.fontCaption}}
        Row{spacing:8;Rectangle{width:8;height:8;radius:4;color:adminController.databaseReady?Theme.success:Theme.danger;anchors.verticalCenter:parent.verticalCenter}Text{text:"数据库";color:Theme.textSecondary;font.pixelSize:Theme.fontCaption}}
        Rectangle{width:1;height:24;color:Theme.borderSubtle}
        Text{text:adminController.currentTime;color:Theme.textMuted;font.pixelSize:Theme.fontCaption}
        Rectangle{width:34;height:34;radius:17;color:Theme.accent;Text{anchors.centerIn:parent;text:(adminController.administrator||"A").charAt(0).toUpperCase();color:"white";font.bold:true}}
        Column{Text{text:adminController.administrator||"admin";color:Theme.textPrimary;font.pixelSize:Theme.fontBody;font.bold:true}Text{text:"系统管理员";color:Theme.textMuted;font.pixelSize:Theme.fontCaption}}
    }
}

