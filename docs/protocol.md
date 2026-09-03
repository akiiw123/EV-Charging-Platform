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
- `station.list`：返回所有电站、总桩数和空闲桩数。
- `station.detail`：`payload.station_id` 为电站 ID；返回电站及电桩明细。
- `pile.list`：`payload.station_id` 为电站 ID；返回该站电桩列表。
- `user.profile` / `user.profile.update`：查询或修改当前连接已登录用户资料。
- `wallet.recharge`：为当前用户模拟充值，金额范围为 0 到 100000 元。
- `order.active` / `order.history`：查询当前未完成订单或最近 50 条订单。
- `order.reserve`：预约空闲电桩；当前用户或电桩已有活动订单时拒绝。
- `order.start` / `order.stop`：开始或停止充电，停止时由服务端计算电量和费用。
- `order.settle` / `order.cancel`：钱包结算待付款订单或取消预约。

用户相关接口绑定当前 TCP 连接的登录身份，不接受客户端提交任意用户 ID。连接重建后必须重新登录。

## 管理接口

- `admin.login`：管理员账号密码登录，开发环境默认 `admin / 123456`。
- `admin.dashboard`：今日、本月、累计营收，电桩状态分布和近30日营收趋势；`payload.days` 可选 7/30 指定趋势区间。
- `admin.station.list` / `admin.station.create`：电站查询和新增，并可批量初始化电桩。
- `admin.pile.list` / `admin.pile.restart`：电桩明细和模拟远程重启。
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
