/*
 * 文件职责：空状态组件：在无数据、加载失败等场景提供统一图标、说明和操作入口。
 * 对接关系：属于 AdminAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import Charging.UI
Item{property string message:"暂无数据";Text{anchors.centerIn:parent;text:parent.message;color:Theme.textMuted;font.pixelSize:Theme.fontBody}}

