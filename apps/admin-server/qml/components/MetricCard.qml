/*
 * 文件职责：指标卡：展示数值、单位、趋势和语义状态。
 * 对接关系：属于 AdminAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import QtQuick.Layouts
import Charging.UI
PanelCard{id:root;property string label;property string value;property string unit;property string trend;property color tone:Theme.accent;implicitHeight:112
    ColumnLayout{anchors.fill:parent;anchors.margins:16;spacing:5
        RowLayout{Layout.fillWidth:true;Text{text:root.label;color:Theme.textSecondary;font.pixelSize:Theme.fontCaption}Item{Layout.fillWidth:true}Rectangle{width:28;height:28;radius:8;color:Qt.rgba(root.tone.r,root.tone.g,root.tone.b,.14);Rectangle{width:8;height:8;radius:4;color:root.tone;anchors.centerIn:parent}}}
        Row{spacing:5;Text{text:root.value;color:Theme.textPrimary;font.pixelSize:Theme.fontDisplay;font.weight:Font.Bold}Text{text:root.unit;color:Theme.textMuted;font.pixelSize:Theme.fontCaption;anchors.baseline:parent.children[0].baseline}}
        Text{text:root.trend;color:root.trend.indexOf("-")===0?Theme.danger:Theme.success;font.pixelSize:Theme.fontCaption;visible:text.length>0}
    }}

