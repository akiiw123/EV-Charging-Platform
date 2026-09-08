// 集中定义状态值、输入边界和计费等纯业务规则，避免规则散落在界面或路由中。
#pragma once

#include <QtGlobal>

// 充电业务规则常量(全局唯一的取值来源,服务端所有相关判断引用此处)

namespace charging::core {

// 预约保留时长(分钟):超时未开始充电将被系统自动取消并释放电桩。
// 历史订单中"用户取消"与"系统超时取消"统一存储为 cancelled,
// 面向用户的失败提示按创建时间是否超出本窗口来区分口径。
constexpr int kReservationTimeoutMinutes = 15;

} // namespace charging::core
