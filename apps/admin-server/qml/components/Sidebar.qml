import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Charging.UI

// 深海军蓝侧边栏(参考 ChargePilot 风格):选中项为品牌蓝填充圆角块 + 白字,
// 非选中项白/灰字;悬停仅轻微提亮,不使用左侧指示条与浅色底
Rectangle {
    id: root
    property int currentIndex: 0
    signal selected(int index)
    width: adminController.sidebarExpanded?232:76
    color: "#0A112E"
    Behavior on width{NumberAnimation{duration:Theme.durationNormal;easing.type:Easing.OutCubic}}
    ColumnLayout{anchors.fill:parent;spacing:0
        Item{Layout.fillWidth:true;Layout.preferredHeight:72
            Rectangle{width:36;height:36;radius:11;color:Theme.accent;anchors.left:parent.left;anchors.leftMargin:20;anchors.verticalCenter:parent.verticalCenter
                LineIcon{anchors.centerIn:parent;name:"pile";strokeColor:"white"}}
            Column{visible:adminController.sidebarExpanded;anchors.left:parent.left;anchors.leftMargin:68;anchors.verticalCenter:parent.verticalCenter
                Text{text:"充电运营平台";color:"#FFFFFF";font.pixelSize:15;font.bold:true}
                Text{text:"运营管理端";color:"#5E6C8A";font.pixelSize:10}}
        }
        Repeater{model:[{t:"数据总览",i:"dashboard"},{t:"电站管理",i:"station"},{t:"电桩管理",i:"pile"},{t:"订单管理",i:"order"},{t:"用户管理",i:"user"},{t:"智能预测",i:"chart"},{t:"主题与设置",i:"settings"}]
            delegate:Item{required property var modelData;required property int index;Layout.fillWidth:true;Layout.preferredHeight:48
                Rectangle{anchors.fill:parent;anchors.margins:6;radius:Theme.radiusSmall
                    color:root.currentIndex===index?Theme.accent:mouse.containsMouse?"#141C3F":"transparent"
                    LineIcon{anchors.left:parent.left;anchors.leftMargin:16;anchors.verticalCenter:parent.verticalCenter;name:modelData.i;strokeColor:root.currentIndex===index?"white":mouse.containsMouse?"#C6D0E4":"#8E99B4"}
                    Text{visible:adminController.sidebarExpanded;anchors.left:parent.left;anchors.leftMargin:52;anchors.verticalCenter:parent.verticalCenter;text:modelData.t;color:root.currentIndex===index?"white":mouse.containsMouse?"#C6D0E4":"#AAB4CC";font.pixelSize:Theme.fontBody;font.weight:root.currentIndex===index?Font.DemiBold:Font.Normal}
                    ToolTip.visible:mouse.containsMouse&&!adminController.sidebarExpanded;ToolTip.text:modelData.t
                    MouseArea{id:mouse;anchors.fill:parent;hoverEnabled:true;cursorShape:Qt.PointingHandCursor;onClicked:root.selected(index)}
                }
            }
        }
        Item{Layout.fillHeight:true}
        Item{Layout.fillWidth:true;Layout.preferredHeight:52
            LineIcon{anchors.left:parent.left;anchors.leftMargin:27;anchors.verticalCenter:parent.verticalCenter;name:"logout";strokeColor:"#8E99B4"}
            Text{visible:adminController.sidebarExpanded;anchors.left:parent.left;anchors.leftMargin:58;anchors.verticalCenter:parent.verticalCenter;text:"退出登录";color:"#AAB4CC";font.pixelSize:Theme.fontBody}
            MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:adminController.logout()}
        }
    }
}
