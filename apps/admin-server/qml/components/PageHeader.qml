import QtQuick
import QtQuick.Layouts
import Charging.UI
RowLayout{property string title;property string subtitle;spacing:12
    ColumnLayout{Layout.fillWidth:true;spacing:3;Text{text:parent.parent.title;color:Theme.textPrimary;font.pixelSize:Theme.fontTitle;font.weight:Font.DemiBold}Text{text:parent.parent.subtitle;color:Theme.textMuted;font.pixelSize:Theme.fontCaption;visible:text.length>0}}
}

