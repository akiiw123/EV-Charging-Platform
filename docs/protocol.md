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

## 错误码

- `INVALID_MESSAGE`：不是有效的一行 JSON，或缺少 `id/type`。
- `INVALID_ARGUMENT`：请求参数不存在或类型错误。
- `AUTH_INVALID_PHONE`：手机号格式错误或注册失败。
- `AUTH_USER_FROZEN`：用户被冻结。
- `STATION_NOT_FOUND`：电站不存在。
- `DATABASE_ERROR`：数据库操作失败。
- `UNKNOWN_REQUEST`：不支持的请求类型。

服务端使用受限线程池处理连接，每个工作线程创建独立 SQLite 连接，避免跨线程共享 `QSqlDatabase`。
