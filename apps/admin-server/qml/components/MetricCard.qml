import QtQuick
import QtQuick.Layouts
import Charging.UI
PanelCard{id:root;property string label;property string value;property string unit;property string trend;property color tone:Theme.accent;implicitHeight:112
    ColumnLayout{anchors.fill:parent;anchors.margins:16;spacing:5
        RowLayout{Layout.fillWidth:true;Text{text:root.label;color:Theme.textSecondary;font.pixelSize:Theme.fontCaption}Item{Layout.fillWidth:true}Rectangle{width:28;height:28;radius:8;color:Qt.rgba(root.tone.r,root.tone.g,root.tone.b,.14);Rectangle{width:8;height:8;radius:4;color:root.tone;anchors.centerIn:parent}}}
        Row{spacing:5;Text{text:root.value;color:Theme.textPrimary;font.pixelSize:Theme.fontDisplay;font.weight:Font.Bold}Text{text:root.unit;color:Theme.textMuted;font.pixelSize:Theme.fontCaption;anchors.baseline:parent.children[0].baseline}}
        Text{text:root.trend;color:root.trend.indexOf("-")===0?Theme.danger:Theme.success;font.pixelSize:Theme.fontCaption;visible:text.length>0}
    }}

