import QtQuick
import QtQuick.Controls
import Charging.UI
ComboBox{id:control;property string emptyText:"全部";implicitWidth:150;implicitHeight:Theme.controlHeight;font.pixelSize:Theme.fontBody
    contentItem:Text{text:control.displayText;color:Theme.textPrimary;verticalAlignment:Text.AlignVCenter;leftPadding:12;font:control.font}
    background:Rectangle{radius:Theme.radiusSmall;color:Theme.backgroundSecondary;border.color:control.activeFocus?Theme.focusRing:Theme.borderSubtle}
    delegate:ItemDelegate{width:control.width;text:modelData;contentItem:Text{text:parent.text;color:Theme.textPrimary;font.pixelSize:Theme.fontBody;verticalAlignment:Text.AlignVCenter} background:Rectangle{color:parent.hovered?Theme.surfaceHover:Theme.surface}}
    popup.background:Rectangle{color:Theme.surface;border.color:Theme.borderSubtle;radius:Theme.radiusSmall}}
