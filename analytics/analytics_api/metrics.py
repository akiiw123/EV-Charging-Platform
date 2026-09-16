"""解释分析指标口径，并从当前 ETL 批次读取可追溯的来源信息。

输入：能查询 ``etl_metadata`` 的只读仓库。
输出/接口：``metadata(repository)`` 返回批次号、生成时间、质量信息和指标释义。
"""
import json

DEFINITIONS = {
    "sessions": "状态为 charging/awaiting_payment/completed 的充电订单数",
    "total_fee": "仅已完成订单 amount + occupancy_fee；未完成金额不计入营收",
    "abnormal_rate": "原始订单中被清洗、去重或关联校验剔除的比例",
    "utilization_rate": "相对充电负载：充电次数 / 同组最大次数 ×100，非时间利用率",
    "daily_kwh": "桩型总电量 / 实际桩数 / 观测日期跨度天数",
    "battery_health": "起始 SOC 分布，不代表电池健康诊断；缺 SOC 时无数据",
    "platforms": "各下单平台的去重用户数；同一用户可出现在多个平台",
    "area_costs": "成本按配置的电量单价估算，并非实测经营成本",
    "user_radar": "等级均值的 min-max 归一化；无差异维度为 null，不伪造为0",
    "week_compare": "按 Asia/Shanghai 的周六/周日区分，不含法定节假日和调休",
}


def metadata(repository):
    row = repository.fetch_one("SELECT batch_id,payload FROM etl_metadata WHERE singleton=1")
    result = json.loads(row["payload"]) if row else {}
    result["batch_id"] = row["batch_id"] if row else None
    definitions = dict(DEFINITIONS)
    if result.get("metric_profile") == "ncs_teacher_dataset_v1":
        definitions.update({
            "sessions": "DWD 清洗后保留的充电订单数",
            "total_fee": "源数据 charging_fees 合计；零值按真实数据保留",
            "abnormal_rate": "清洗后订单中缺少 SOC/BMS 过程明细的比例",
            "daily_kwh": "桩型总电量 / 源数据设备数，数据日期不可靠时不解释为日均",
            "platforms": "各下单平台的充电会话数",
            "week_compare": "按源数据 weekday 划分工作日与周末，不含节假日调休",
        })
    result["definitions"] = definitions
    return result
