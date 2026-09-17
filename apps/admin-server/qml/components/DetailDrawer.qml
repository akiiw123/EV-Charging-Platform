/*
 * 文件职责：详情抽屉：在不离开列表页的情况下展示选中记录的完整信息。
 * 对接关系：属于 AdminAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import QtQuick.Controls
import Charging.UI
Drawer{id:drawer;edge:Qt.RightEdge;width:Math.min(460,parent?parent.width*.42:460);height:parent?parent.height:720;modal:true;padding:0
    background:Rectangle{color:Theme.surfaceElevated;border.color:Theme.borderSubtle}
    enter:Transition{NumberAnimation{property:"position";from:0;to:1;duration:Theme.durationNormal;easing.type:Easing.OutCubic}}
    exit:Transition{NumberAnimation{property:"position";from:1;to:0;duration:Theme.durationNormal;easing.type:Easing.InCubic}}
}

