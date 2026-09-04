# glm 分支变更说明(相对 main)

> 更新日期:2026-09-04 · 基线:main @ 4627447 · 本分支共 20 个提交
> 缺口来源:`docs/improvement-plan.md`(逐项代码取证复查)

## 一、功能新增

### 1. 智能预测真正接入(ml 前后端打通)
- `apps/admin-server/src/admin_app_controller.*`:`refreshPredictions()` 改为
  GET `/stations` → 逐站点并发 POST `/predict`(带超时),表格显示真实
  1/6/24h 负荷、预计空闲桩与高峰/容量风险;指标卡显示全站合计与可信度。
- 服务不可达自动降级演示数据(原行为保留);单站失败不影响其余站点。
- 新增文档:`docs/improvement-plan.md`。

### 2. 管理员初始密码改密流程
- 新协议 `admin.password.change`(校验旧密码 → ≥8 位且不得与当前相同 →
  PBKDF2-SHA256 落库并清除首登标志);仓储层新增
  `AdministratorRepository::findById / changePassword`。
- 管理端登录后弹改密提醒,可"稍后再说"/Esc 跳过,每次登录会再提醒;
  改密成功后不再提醒。

### 3. 用户端头像
- `pickAvatar()`:文件选择器 → 校验(png/jpg/bmp,≤5MB)→ 居中裁方缩放
  256×256 → 圆形透明 PNG 存入应用数据目录(按手机号命名)→
  `user.profile.update` 持久化;界面图片优先、昵称首字母兜底,即时预览。

### 4. 用户端真实定位(B1)
- 6 个内置城市快选芯片(模拟 GPS/区域选择,离线可用)+ 当前经纬度读数;
- 配置 `TENCENT_MAP_KEY` 后任意地址经腾讯 WebService geocoder 解析;
  无 Key/解析失败明确降级提示;距离排序从此使用真实坐标。

### 5. 电桩手工状态管理与单独增删(C2)
- 新协议 `admin.pile.create / admin.pile.update / admin.pile.status`;
  仓储层新增 `PileRepository::update`。
- 电桩管理页:新增电桩对话框(电站下拉/编号/类型/功率)、抽屉内状态切换
  (空闲/故障/离线)与编辑入口;**充电中的电桩一律拒绝**相关操作。

### 6. 预约超时自动释放
- 预约超 15 分钟未开始自动取消(请求入口统一清理),释放用户/电桩配额。

### 7. 活动订单强制引导
- 登录后检测到未完成订单弹出提醒(支持"去处理"直达充电页,每次登录提醒一次)。

### 8. Web 大屏扩展(E1+E3)
- **ECharts 本地化**:`web/dashboard/echarts.min.js` 随目录分发,离线可渲染;
- 新增:桩位利用率、站点营收排行(前5)、近 7 日 24 小时时段分布,
  营收趋势扩至 30 天并在前端切换 近7日/近30日;
- 修复 `server.py` 默认数据库路径按工作目录解析导致的启动 500,
  现按脚本位置解析,任意目录可启动。

## 二、正确性修复

- **营收趋势时区**(P0):30 日趋势 SQL 统一按本地时区截断(原 UTC 边界丢凌晨订单)。
- **并发旧响应覆盖(B4)**:ApiClient 按请求类型维护单调序号,同类型旧响应
  改发 `staleResponseReceived`(仅归还 busy),不再污染界面状态;
  断线重连清空序号簿。
- **冻结踢出已建立会话(C4)**:被冻结用户的会话在下一个鉴权请求被拒
  (`AUTH_USER_FROZEN`)并断开;空闲会话约 5 秒轮询踢出(`server.session.closed`)。

## 三、UI 去 AI 化(参考 ChargePilot 截图提取令牌)

- 配色:青色 #0F9F8F → **信号蓝 #1B6EF3**(管理端+用户端统一品牌),
  中性灰底、藏青墨文字;语义色绿/琥珀/红降饱和。
- 管理端侧边栏:深海军蓝 #0A112E + 品牌蓝填充式选中项(白字)。
- 字符图标(`ϟ ● ⎂ ✎ ○`)全部替换为 Canvas 矢量图标(`AppIcon.qml`);
  5 处装饰渐变拉平为纯色;侧边栏 `OPERATIONS` 装饰字与营销腔文案移除;
  圆角 24/16/10 收紧为 20/14/8。

## 四、清理与文档

- 删除 `apps/user-client/src/views/`(12 个未参与编译的旧 Widgets 视图)。
- 新增/更新文档:`docs/improvement-plan.md`(缺口清单+复查快照)、
  `docs/manual-testing.md`(手动测试指南,约 55 个用例)、
  `docs/protocol.md`(新接口与会话语义)、`docs/changes-glm.md`(本文)。

## 五、测试与验证基线

- 自动化:3 套件 **15 个用例全部通过**(新增 预约超时 / 改密 / 头像 /
  电桩管理 / dashboard days / stale 响应 / 冻结踢会话 两级)。
- 手动:双端编译零错误零警告;GUI 启动冒烟(offscreen)正常;
  大屏 API 字段端到端验证通过。
- 本机 QML 运行时依赖(仅 192.168.202.128):`qml6-module-qtquick-templates
  qml6-module-qtquick-window qml6-module-qtwebengine`,或
  `export QML_IMPORT_PATH=~/qt-extra-qml/usr/lib/x86_64-linux-gnu/qt6/qml`。
