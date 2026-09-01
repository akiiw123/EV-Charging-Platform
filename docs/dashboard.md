# Web 运营大屏

大屏使用 Python 标准库提供只读 HTTP API，并直接服务 `web/dashboard` 静态文件，不需要安装额外依赖。

先在项目根目录启动或运行一次管理端，使 `charging_platform.db` 存在，然后执行：

```bash
cd /home/bit/charging-platform
python3 web/dashboard/server.py --database charging_platform.db --host 0.0.0.0 --port 8080
```

在 Windows 浏览器访问：

```text
http://192.168.179.128:8080
```

页面每5秒从 `/api/dashboard` 刷新今日营收、在线电桩、进行中订单、近7日营收趋势和电桩状态分布。API 只执行查询，不直接修改业务数据。

正式部署时应使用反向代理限制来源并启用 HTTPS；当前服务用于局域网课程演示。
