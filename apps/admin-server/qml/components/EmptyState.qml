// 列表无数据或加载失败时的统一提示。
import QtQuick
import Charging.UI
Item{property string message:"暂无数据";Text{anchors.centerIn:parent;text:parent.message;color:Theme.textMuted;font.pixelSize:Theme.fontBody}}

