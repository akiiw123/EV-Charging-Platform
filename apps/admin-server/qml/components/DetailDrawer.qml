import QtQuick
import QtQuick.Controls
import Charging.UI
Drawer{id:drawer;edge:Qt.RightEdge;width:Math.min(460,parent?parent.width*.42:460);height:parent?parent.height:720;modal:true;padding:0
    background:Rectangle{color:Theme.surfaceElevated;border.color:Theme.borderSubtle}
    enter:Transition{NumberAnimation{property:"position";from:0;to:1;duration:Theme.durationNormal;easing.type:Easing.OutCubic}}
    exit:Transition{NumberAnimation{property:"position";from:1;to:0;duration:Theme.durationNormal;easing.type:Easing.InCubic}}
}

