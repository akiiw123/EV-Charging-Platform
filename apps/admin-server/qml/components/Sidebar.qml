import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Charging.UI

Rectangle {
    id: root
    property int currentIndex: 0
    signal selected(int index)
    width: adminController.sidebarExpanded?232:76
    color: Theme.backgroundSecondary
    border.color: Theme.borderSubtle
    Behavior on width{NumberAnimation{duration:Theme.durationNormal;easing.type:Easing.OutCubic}}
    ColumnLayout{anchors.fill:parent;spacing:0
        Item{Layout.fillWidth:true;Layout.preferredHeight:72
            Rectangle{width:36;height:36;radius:11;color:Theme.accent;anchors.left:parent.left;anchors.leftMargin:20;anchors.verticalCenter:parent.verticalCenter
                LineIcon{anchors.centerIn:parent;name:"pile";strokeColor:"white"}}
            Column{visible:adminController.sidebarExpanded;anchors.left:parent.left;anchors.leftMargin:68;anchors.verticalCenter:parent.verticalCenter
                Text{text:"充电运营平台";color:Theme.textPrimary;font.pixelSize:15;font.bold:true}
                Text{text:"OPERATIONS";color:Theme.textMuted;font.pixelSize:9;font.letterSpacing:1.5}}
        }
        Repeater{model:[{t:"数据总览",i:"dashboard"},{t:"电站管理",i:"station"},{t:"电桩管理",i:"pile"},{t:"订单管理",i:"order"},{t:"用户管理",i:"user"},{t:"智能预测",i:"chart"},{t:"主题与设置",i:"settings"}]
            delegate:Item{required property var modelData;required property int index;Layout.fillWidth:true;Layout.preferredHeight:48
                Rectangle{anchors.fill:parent;anchors.margins:6;radius:Theme.radiusSmall;color:root.currentIndex===index?Theme.surfaceSelected:mouse.containsMouse?Theme.surfaceHover:"transparent"
                    Rectangle{visible:root.currentIndex===index;width:3;height:22;radius:2;color:Theme.accent;anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter}
                    LineIcon{anchors.left:parent.left;anchors.leftMargin:16;anchors.verticalCenter:parent.verticalCenter;name:modelData.i;strokeColor:root.currentIndex===index?Theme.accent:Theme.textSecondary}
                    Text{visible:adminController.sidebarExpanded;anchors.left:parent.left;anchors.leftMargin:52;anchors.verticalCenter:parent.verticalCenter;text:modelData.t;color:root.currentIndex===index?Theme.textPrimary:Theme.textSecondary;font.pixelSize:Theme.fontBody;font.weight:root.currentIndex===index?Font.DemiBold:Font.Normal}
                    ToolTip.visible:mouse.containsMouse&&!adminController.sidebarExpanded;ToolTip.text:modelData.t
                    MouseArea{id:mouse;anchors.fill:parent;hoverEnabled:true;cursorShape:Qt.PointingHandCursor;onClicked:root.selected(index)}
                }
            }
        }
        Item{Layout.fillHeight:true}
        Item{Layout.fillWidth:true;Layout.preferredHeight:52
            LineIcon{anchors.left:parent.left;anchors.leftMargin:27;anchors.verticalCenter:parent.verticalCenter;name:"logout";strokeColor:Theme.danger}
            Text{visible:adminController.sidebarExpanded;anchors.left:parent.left;anchors.leftMargin:58;anchors.verticalCenter:parent.verticalCenter;text:"退出登录";color:Theme.danger;font.pixelSize:Theme.fontBody}
            MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:adminController.logout()}
        }
    }
}

