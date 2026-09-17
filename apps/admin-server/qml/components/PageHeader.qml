/*
 * 文件职责：页面标题栏：统一标题、说明、刷新和新增等页面级操作。
 * 对接关系：属于 AdminAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import QtQuick.Layouts
import Charging.UI
RowLayout{property string title;property string subtitle;spacing:12
    ColumnLayout{Layout.fillWidth:true;spacing:3;Text{text:parent.parent.title;color:Theme.textPrimary;font.pixelSize:Theme.fontTitle;font.weight:Font.DemiBold}Text{text:parent.parent.subtitle;color:Theme.textMuted;font.pixelSize:Theme.fontCaption;visible:text.length>0}}
}

