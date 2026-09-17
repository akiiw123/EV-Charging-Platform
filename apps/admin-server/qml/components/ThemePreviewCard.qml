/*
 * 文件职责：主题预览卡：展示主题色板并将用户选择传给 Controller。
 * 对接关系：属于 AdminAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import QtQuick.Layouts
import Charging.UI
Rectangle{id:root;property string themeId;property string title;property var colors;property bool selected:false;signal chosen();implicitWidth:220;implicitHeight:132;radius:Theme.radiusMedium;color:Theme.surface;border.width:selected?2:1;border.color:selected?Theme.accent:mouse.containsMouse?Theme.borderStrong:Theme.borderSubtle
    ColumnLayout{anchors.fill:parent;anchors.margins:14;spacing:10
        Rectangle{Layout.fillWidth:true;Layout.preferredHeight:60;radius:8;color:colors[0];Row{anchors.centerIn:parent;spacing:7;Repeater{model:root.colors.slice(1);delegate:Rectangle{required property var modelData;width:24;height:24;radius:12;color:modelData}}}}
        Text{text:root.title;color:Theme.textPrimary;font.pixelSize:Theme.fontBody;font.bold:true}
    }MouseArea{id:mouse;anchors.fill:parent;hoverEnabled:true;cursorShape:Qt.PointingHandCursor;onClicked:root.chosen()}}
