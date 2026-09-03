import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Charging.UI
import "../components"

ScrollView{id:root;contentWidth:availableWidth;clip:true;property string previewTheme:adminController.theme
    ColumnLayout{x:24;width:root.availableWidth-48;spacing:18
        PageHeader{Layout.fillWidth:true;title:"主题与设置";subtitle:"主题、字体与表格偏好"}
        Text{text:"界面主题";color:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.bold:true}
        GridLayout{Layout.fillWidth:true;columns:3;columnSpacing:14;rowSpacing:14
            Repeater{model:[{id:"default",t:"默认青绿 Charging Light",c:["#F4F7FA","#FFFFFF","#0F9F8F"]},{id:"midnight",t:"深海青蓝 Midnight Cyan",c:["#07111F","#11223A","#18C8F4"]},{id:"aurora",t:"极光紫 Aurora Violet",c:["#0B1020","#171E35","#8B75FF"]},{id:"graphite",t:"石墨橙 Graphite Orange",c:["#121416","#202429","#F2A93B"]},{id:"emerald",t:"翡翠绿 Emerald Grid",c:["#071713","#112B23","#2ED39A"]},{id:"porcelain",t:"云白蓝 Porcelain Light",c:["#F5F8FC","#FFFFFF","#3478F6"]},{id:"contrast",t:"高对比 High Contrast",c:["#000000","#111111","#00E5FF"]}]
                delegate:ThemePreviewCard{required property var modelData;Layout.fillWidth:true;themeId:modelData.id;title:modelData.t;colors:modelData.c;selected:root.previewTheme===themeId;onChosen:{root.previewTheme=themeId;Theme.currentTheme=themeId}}}
        }
        PanelCard{Layout.fillWidth:true;Layout.preferredHeight:190
            ColumnLayout{anchors.fill:parent;anchors.margins:20;spacing:12
                Text{text:"显示偏好";color:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.bold:true}
                RowLayout{Layout.fillWidth:true;Text{text:"启用界面动画";color:Theme.textSecondary;Layout.fillWidth:true}Switch{checked:adminController.animationsEnabled;onToggled:adminController.animationsEnabled=checked}}
                RowLayout{Layout.fillWidth:true;Text{text:"字体缩放";color:Theme.textSecondary;Layout.preferredWidth:140}Slider{id:scale;Layout.fillWidth:true;from:.85;to:1.3;stepSize:.05;value:adminController.fontScale}Text{text:Math.round(scale.value*100)+"%";color:Theme.textPrimary;Layout.preferredWidth:55}AppButton{text:"应用";variant:"secondary";onClicked:adminController.fontScale=scale.value}}
                RowLayout{Layout.fillWidth:true;Text{text:"表格每页条数";color:Theme.textSecondary;Layout.fillWidth:true}FilterComboBox{id:size;model:["10","20","50","100"];currentIndex:[10,20,50,100].indexOf(adminController.pageSize);onActivated:adminController.pageSize=Number(currentText)}}
            }}
        RowLayout{Layout.fillWidth:true;Item{Layout.fillWidth:true}AppButton{text:"恢复默认";variant:"secondary";onClicked:{root.previewTheme="default";Theme.currentTheme="default";scale.value=1}}AppButton{text:"应用主题";onClicked:adminController.theme=root.previewTheme}}
        Item{height:10}
    }
}
