# TCP/JSON 协议

管理端默认监听 `0.0.0.0:45454`。用户端默认连接 `127.0.0.1:45454`，可通过环境变量覆盖：

```bash
export CHARGING_SERVER_HOST=192.168.179.128
export CHARGING_SERVER_PORT=45454
```

## 帧格式

每行是一条 UTF-8 JSON，单条消息最大 1 MiB：

```json
{"id":"req-001","type":"station.list","payload":{}}
```

成功响应使用 `<type>.ok`，业务错误使用 `<type>.error`。格式错误返回 `protocol.error`。响应始终复用请求 `id`，便于客户端匹配并发请求。

## 已实现请求

- `auth.phone_login`：`payload.phone` 为 11 位手机号；不存在时自动注册。
- `station.list`：返回所有**营业中**电站、总桩数和空闲桩数;逻辑停用(disabled)的电站对用户端不可见。
- `station.detail`：`payload.station_id` 为电站 ID；返回电站及电桩明细。
- `station.pricing`：`payload.station_id` 为电站 ID；只读返回该站收费数据，
  `payload.pricing` 含 `fixed_price_per_kwh`（`charging_stations.price_per_kwh`）、
  `current_price_per_kwh`（按当前时刻解析）、`periods[]`
  （`start_minute`/`end_minute` 左闭右开、`period_type` 为 `peak|flat|valley`、`price_per_kwh`）
  以及 `rule`（`enabled`/`free_move_minutes`/`occupancy_fee_per_minute`/`occupancy_fee_cap`，
  未配置时为 `null`，`occupancy_fee_cap <= 0` 表示占位费不封顶）。
  逻辑停用（disabled）的电站与 `station.detail` 口径一致，返回 `STATION_NOT_FOUND` 错误。
  该接口为新增类型，旧客户端不受影响；`order.stop` 计费仍使用站点固定电价。
- `pile.list`：`payload.station_id` 为电站 ID；返回该站电桩列表。
- `user.profile` / `user.profile.update`：查询或修改当前连接已登录用户资料。
- `wallet.recharge`：为当前用户模拟充值，金额范围为 0 到 100000 元。
- `order.active` / `order.history`：查询当前未完成订单或最近 50 条订单。
- `order.reserve`：预约空闲电桩；当前用户或电桩已有活动订单时拒绝。
- `order.start` / `order.stop`：开始或停止充电，停止时由服务端计算电量和费用。
- `order.settle` / `order.cancel`：钱包结算待付款订单或取消预约。

用户相关接口绑定当前 TCP 连接的登录身份，不接受客户端提交任意用户 ID。连接重建后必须重新登录。

### 预约占用策略(第一阶段)

- 不引入电桩 `reserved` 状态;预约占用由**订单唯一约束**实现:数据库部分唯一索引保证
  每用户/每电桩同时仅一个活动订单(`reserved/charging/awaiting_payment`)。
- 预约超 **15 分钟** 未开始充电,系统在任意请求入口自动取消该预约并释放电桩
  (超时时长为服务端常量 `kReservationTimeoutMinutes`)。
- 对已取消的预约发起 `order.start` / `order.cancel`,会返回明确的超时提示
  (如"预约已超时自动取消(超过 15 分钟未开始),请重新预约")。

### 冻结对在途订单的处理

- 冻结立即禁止登录,并踢出已建立的会话(见下);**不自动取消/结算其进行中订单**。
- 充电中订单:踢出后服务端不再收到该用户的停止请求,订单保持 `charging`;
  管理员可通过"编辑/切换状态受限"知悉,解冻后用户可重新登录继续操作。
- 待结算订单:保留至解冻后由用户自行结算;期间该用户与对应电桩仍被唯一约束占用。
- 以上口径保证不会产生"无人可结算"的孤儿订单(订单始终归属可恢复的账号)。

### 重复请求保护(第一阶段)

- 客户端:请求期间全局 busy 遮罩/按钮禁用,防止双击重复提交。
- 服务端:关键操作依赖状态条件(重复 `order.settle` 返回"订单已完成结算,请勿重复操作";
  重复 `order.reserve` 返回 `ORDER_ACTIVE_EXISTS`),不引入完整幂等表。

被冻结的用户会话会被服务端主动断开：用户发出下一个鉴权请求时立即拒绝
（`AUTH_USER_FROZEN`）并断开；即使不发请求，服务端约每 5 秒轮询一次，
发送 `server.session.closed` 后断开。客户端可自动重连，但重新登录仍会被拒。

### 单点登录（同一车主账号仅一个客户端在线）

服务端在 `TcpServer` 层维护一张跨连接共享的**车主在线会话表**
（`SessionRegistry`，互斥锁保护），限制同一账号同时只能在一个客户端处于登录态：

- `auth.phone_login` 成功时，该连接**接管**账号并取得一枚单调递增的会话令牌；
  此前持有该账号的连接令牌随即失效。策略为**后登录优先**，不拒绝新登录。
- 被接管的旧连接通过两条路径下线，两者都携带 `AUTH_SESSION_TAKEOVER`：
  - 请求级：下一个鉴权请求（`user.*` / `wallet.*` / `order.*`）立即返回
    `<type>.error`，随后断开；
  - 轮询级：令牌复查为纯内存操作，服务端每个空闲周期（约 250 ms）复查一次，
    命中后主动下发 `server.session.closed` 再断开。
- 连接结束（客户端断开、服务停止或被踢下线）时释放占用；释放会校验令牌归属，
  因此旧连接迟到的析构不会误删接管者的占用，被踢下线的客户端可立即重新登录。
- 会话关闭后，同一次读取中缓冲的后续请求一律拒绝，不会以旧身份继续操作。
- **仅约束车主账号**；`admin.login` 的管理员会话相互独立，同一管理员账号可在
  多个管理端同时在线。
- 在途订单不受影响：接管只结束旧连接的登录态，订单状态仍由数据库持有，
  新连接可用 `order.active` 继续充电/结算流程（与冻结踢会话的口径一致）。
- 在线状态保存在服务端内存中，**不落库**；服务端重启后所有连接都需重新登录。
  当前没有 `auth.logout` 请求，用户端"退出登录"仅清理本地状态，
  服务端占用随该 TCP 连接断开而释放。

## 管理接口

- `admin.login`：管理员账号密码登录，开发环境默认 `admin / 123456`。
- `admin.dashboard`：今日/本月/累计营收、已完成订单数(今日/累计)、平均订单金额、注册用户数、电桩状态分布、在线率、近7/30日营收趋势(缺数据日期补0,日期连续);`payload.days` 可选 7/30 指定趋势区间。营收与订单数口径均只统计 `completed` 订单。
- `admin.station.list` / `admin.station.create`：电站查询(含已停用,带营业状态)和新增，并可批量初始化电桩。
- `admin.station.update`：编辑电站资料;可选 `payload.status`(`active`/`disabled`)实现**逻辑停用/恢复营业**——
  停用后用户端不再展示该电站、其电桩不可预约(服务端在 `order.reserve` 兜底校验);
  历史订单与数据保留,可随时恢复。删除接口仍保留,有活动订单时拒绝。
- `admin.pile.list` / `admin.pile.restart`：电桩明细和模拟远程重启。
- `admin.pile.create`：单独新增电桩，`payload.station_id/code/type(fast|slow)/power_kw(0,1000]`；
  编号全局唯一，重复返回 `PILE_CREATE_FAILED`。
- `admin.pile.update`：编辑类型与功率，`payload.pile_id/type/power_kw`；充电中拒绝（`PILE_UPDATE_FAILED`）。
- `admin.pile.status`：手工切换状态，`payload.pile_id/status(idle|fault|offline)`；充电中拒绝（`PILE_STATUS_FAILED`）。
- `admin.user.list` / `admin.user.status`：手机号模糊搜索及用户冻结、解冻。
- `admin.password.change`：`payload.old_password` / `new_password`；校验当前密码后将新密码以
  PBKDF2-SHA256 落库并清除首登改密标志。新密码至少 8 位且不得与当前密码相同，
  错误码 `PASSWORD_WEAK` / `PASSWORD_OLD_MISMATCH`。

管理员身份同样绑定当前 TCP 连接，与车主登录会话相互独立。

## 错误码

- `INVALID_MESSAGE`：不是有效的一行 JSON，或缺少 `id/type`。
- `INVALID_ARGUMENT`：请求参数不存在或类型错误。
- `AUTH_INVALID_PHONE`：手机号格式错误或注册失败。
- `AUTH_USER_FROZEN`：用户被冻结。
- `AUTH_SESSION_TAKEOVER`：账号已在其他客户端登录，本连接被服务端接管下线。
- `AUTH_REQUIRED`：连接尚未完成登录。
- `ORDER_ACTIVE_EXISTS`：用户已有未完成订单。
- `ORDER_NOT_FOUND`：订单不存在或不属于当前用户。
- `ORDER_START_FAILED` / `ORDER_STOP_FAILED` / `ORDER_SETTLE_FAILED`：订单状态或余额不满足操作条件。
- `ADMIN_AUTH_REQUIRED` / `ADMIN_LOGIN_FAILED`：管理员未登录或凭据错误。
- `STATION_CREATE_FAILED`：新增电站或初始化电桩失败。
- `PILE_RESTART_FAILED`：电桩不存在或处于充电状态。
- `USER_STATUS_FAILED`：冻结、解冻用户失败。
- `STATION_NOT_FOUND`：电站不存在。
- `DATABASE_ERROR`：数据库操作失败。
- `UNKNOWN_REQUEST`：不支持的请求类型。

服务端使用受限线程池处理连接，每个工作线程创建独立 SQLite 连接，避免跨线程共享 `QSqlDatabase`。
跨连接的车主在线会话表（`SessionRegistry`）是唯一被所有连接线程共享的状态，由互斥锁保护。
