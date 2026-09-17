/*
 * 文件职责：搜索输入组件：统一清空、提交和防抖相关交互。
 * 对接关系：属于 AdminAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import Charging.UI
AppTextField{placeholderText:"搜索";implicitWidth:220}
