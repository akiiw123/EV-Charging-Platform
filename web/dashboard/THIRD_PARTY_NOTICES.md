# 第三方资源与来源说明

Vue3 大屏源码是针对本项目编写的实现，不包含商业闭源产品的私有代码、付费模板或素材。

前端依赖通过 `package-lock.json` 固定版本：Vue 3 与 `@kjgl77/datav-vue3` 使用 MIT 许可证，Apache ECharts 使用 Apache-2.0。仓库不再提交生成后的 ECharts bundle；依赖许可证随 npm 包保留。

原项目地图 `assets/china.json` 和 Logo `assets/voltflow-logo.png` 原样用于用户项目改造及离线演示。原项目文档将底图描述为 DataV GeoAtlas 来源的教学资源；生产分发前仍应由项目方确认地图数据的授权、坐标系和适用要求。

GoView、Grafana、阿里云 DataV、FineVis 仅作为研究参考；本项目没有复制这些产品的商业资产。不能据本说明推断这些项目采用相同许可证，或推断商业模板可直接再分发。

未打包任何字体文件、用户生产数据库、原项目账户数据或 Qt 编译产物。

## V3 真实站点数据

站点资料来自 OpenStreetMap，© OpenStreetMap contributors，采用 ODbL 1.0。许可与署名说明：https://www.openstreetmap.org/copyright 。提交内容只有前端所需的静态 JSON，不包含 SQLite 数据库。
