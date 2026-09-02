# 用户端运行与验收

## 运行

先启动管理端和 TCP 服务：

~~~bash
cd /home/bit/charging-platform
./build/dev/apps/admin-server/charging-admin
~~~

再打开一个 VS Code 远程终端启动用户端：

~~~bash
cd /home/bit/charging-platform
export CHARGING_SERVER_HOST=127.0.0.1
export CHARGING_SERVER_PORT=45454
export TENCENT_MAP_KEY=你的腾讯地图Key
./build/dev/apps/user-client/charging-user
~~~

TENCENT_MAP_KEY 不写入仓库。腾讯 URI 路线规划在部分环境下可以不传 Key；建议演示时仍配置已授权的 Key。

## 演示账号

| 场景 | 手机号 | 预置状态 |
|---|---|---|
| 有余额 | 18800000001 | 余额 200 元，可完成预约充电 |
| 待结算 | 18800000002 | 有一笔 9.45 元待结算订单 |
| 低余额 | 18800000003 | 余额 0.50 元 |
| 已冻结 | 18800000004 | 登录会被服务端拒绝 |

演示数据采用 INSERT OR IGNORE，不会覆盖这些账号后续产生的钱包和订单变化。

## 页面结构

- 登录页：手机号免密登录和演示账号提示。
- 首页：位置/地址搜索、按距离排序的充电站卡片。
- 电站详情：站点指标、电桩状态、预约、驾车/步行导航。
- 充电页：活动订单、开始、停止、取消和钱包结算。
- 我的：用户资料、余额充值、历史订单和退出登录。
- 底部导航：首页、充电、我的。

## 定位说明

当前为课程项目的模拟定位，支持深圳、北京、沈阳三个城市中心点；其他地址默认以深圳市中心为当前位置。站点距离使用经纬度和 Haversine 公式在本地计算。

## 地图组件

导航页使用 QWebEngineView 内嵌腾讯地图，因此构建用户端前必须安装完整的 Qt WebEngine 开发组件。

Ubuntu 22.04 的完整依赖包括：

~~~bash
sudo apt install qt6-webengine-dev qt6-webengine-dev-tools
~~~

## 验收建议

1. 使用有余额账号登录，确认首页站点按距离显示。
2. 进入深圳演示充电站，预约 SZ001-01。
3. 切换“充电”标签，完成开始、停止和结算。
4. 切换“我的”，确认余额和订单历史更新。
5. 分别点击驾车/步行导航，确认起点和目标坐标传入腾讯地图。
6. 使用冻结账号登录，确认显示冻结提示且不能进入主界面。
