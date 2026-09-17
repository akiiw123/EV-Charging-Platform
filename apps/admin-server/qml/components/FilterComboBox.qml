/*
 * 文件职责：单选筛选框：统一下拉列表、当前值和焦点交互。
 * 对接关系：属于 AdminAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import QtQuick.Controls
import Charging.UI
ComboBox{id:control;property string emptyText:"全部";implicitWidth:150;implicitHeight:Theme.controlHeight;font.pixelSize:Theme.fontBody
    contentItem:Text{text:control.displayText;color:Theme.textPrimary;verticalAlignment:Text.AlignVCenter;leftPadding:12;font:control.font}
    background:Rectangle{radius:Theme.radiusSmall;color:Theme.backgroundSecondary;border.color:control.activeFocus?Theme.focusRing:Theme.borderSubtle}
    delegate:ItemDelegate{width:control.width;text:modelData;contentItem:Text{text:parent.text;color:Theme.textPrimary;font.pixelSize:Theme.fontBody;verticalAlignment:Text.AlignVCenter} background:Rectangle{color:parent.hovered?Theme.surfaceHover:Theme.surface}}
    popup.background:Rectangle{color:Theme.surface;border.color:Theme.borderSubtle;radius:Theme.radiusSmall}}
