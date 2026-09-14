-- =====================================================================
-- NCS 充电桩项目  ADS 应用层（spark-sql）—— 大屏 10 张结果表
-- 文件：03_ads_dashboard.sql
-- 来源：ncs_dws.*（由 DWD 汇总）；输出表名/列名与 Flask、大屏前端保持一致
-- 口径说明（真实可算，不造假数据）：
--   * total_fee 直接取订单 charging_fees；该列 89% 为 0（费用字段缺失），如实呈现。
--   * area_cost.cost 为“电量成本”，= 总电量 × 成本单价参数(默认 0.6 元/度，
--     可按当地电价调整本常量)；profit=revenue-cost，属业务建模派生值。
--   * utilization_rate 为“相对负载率”=站点(或桩型)充电次数 / 最大次数 ×100。
--   * abnormal_rate 为数据质量指标 = 缺失 BMS 监测明细(soc 为空)的订单占比。
--   * daily_kwh 因数据集为单批次快照无可靠日期跨度，口径取“单桩平均贡献电量”。
-- 执行：spark-sql -f 03_ads_dashboard.sql  或  sh 03_etl_ads.sh
-- =====================================================================
SET spark.sql.shuffle.partitions = 20;

CREATE DATABASE IF NOT EXISTS ncs_ads;

-- ---------------------------------------------------------------------
-- 1. ads_kpi_overview：顶部 5 个 KPI（单行）
-- ---------------------------------------------------------------------
DROP TABLE IF EXISTS ncs_ads.ads_kpi_overview;
CREATE EXTERNAL TABLE ncs_ads.ads_kpi_overview (
  sessions        BIGINT,
  total_kwh       DOUBLE,
  total_fee       DOUBLE,
  station_count   BIGINT,
  abnormal_rate   DOUBLE
)
STORED AS ORC
LOCATION '/user/hive/warehouse/ncs_ads.db/ads_kpi_overview';

INSERT OVERWRITE TABLE ncs_ads.ads_kpi_overview
SELECT
    COUNT(*)                                                       AS sessions,
    ROUND(SUM(kwh),2)                                              AS total_kwh,
    ROUND(SUM(total_fee),2)                                        AS total_fee,
    COUNT(DISTINCT station_id)                                     AS station_count,
    ROUND(SUM(CASE WHEN soc IS NULL THEN 1 ELSE 0 END)/COUNT(*)*100,2) AS abnormal_rate
FROM ncs_dwd.dwd_charge_detail;

-- ---------------------------------------------------------------------
-- 2. ads_user_level_dist：用户分级分布（按累计充电次数分级）
-- ---------------------------------------------------------------------
DROP TABLE IF EXISTS ncs_ads.ads_user_level_dist;
CREATE EXTERNAL TABLE ncs_ads.ads_user_level_dist (
  user_level STRING, user_count BIGINT
)
STORED AS PARQUET LOCATION '/user/hive/warehouse/ncs_ads.db/ads_user_level_dist';

INSERT OVERWRITE TABLE ncs_ads.ads_user_level_dist
SELECT CASE WHEN charge_count >= 10 THEN '高频用户'
            WHEN charge_count >= 3  THEN '中频用户'
            ELSE '低频用户' END AS user_level,
       COUNT(*) AS user_count
FROM ncs_dws.dws_user_agg
GROUP BY CASE WHEN charge_count >= 10 THEN '高频用户'
              WHEN charge_count >= 3  THEN '中频用户'
              ELSE '低频用户' END;

-- ---------------------------------------------------------------------
-- 3. ads_user_radar：各用户等级在 4 个维度的 min-max 归一化值(0-100)
-- ---------------------------------------------------------------------
DROP TABLE IF EXISTS ncs_ads.ads_user_radar;
CREATE EXTERNAL TABLE ncs_ads.ads_user_radar (
  user_level STRING, dim_name STRING, dim_value DOUBLE
)
STORED AS PARQUET LOCATION '/user/hive/warehouse/ncs_ads.db/ads_user_radar';

INSERT OVERWRITE TABLE ncs_ads.ads_user_radar
WITH lv AS (
  SELECT CASE WHEN charge_count >= 10 THEN '高频用户'
              WHEN charge_count >= 3  THEN '中频用户'
              ELSE '低频用户' END AS user_level,
         charge_count, total_kwh, total_fee, avg_kwh
  FROM ncs_dws.dws_user_agg
), g AS (
  SELECT user_level,
         AVG(charge_count) AS avg_cnt, AVG(total_kwh) AS avg_kwh,
         AVG(total_fee)    AS avg_fee, AVG(avg_kwh)   AS avg_single
  FROM lv GROUP BY user_level
), b AS (
  SELECT MAX(avg_cnt) mx_cnt, MIN(avg_cnt) mn_cnt,
         MAX(avg_kwh) mx_kwh, MIN(avg_kwh) mn_kwh,
         MAX(avg_fee) mx_fee, MIN(avg_fee) mn_fee,
         MAX(avg_single) mx_s,  MIN(avg_single) mn_s
  FROM g
)
SELECT user_level, '充电频次',
       ROUND((avg_cnt-mn_cnt)/NULLIF(mx_cnt-mn_cnt,0)*100,1) FROM g CROSS JOIN b
UNION ALL
SELECT user_level, '累计电量',
       ROUND((avg_kwh-mn_kwh)/NULLIF(mx_kwh-mn_kwh,0)*100,1) FROM g CROSS JOIN b
UNION ALL
SELECT user_level, '消费金额',
       ROUND((avg_fee-mn_fee)/NULLIF(mx_fee-mn_fee,0)*100,1) FROM g CROSS JOIN b
UNION ALL
SELECT user_level, '单次电量',
       ROUND((avg_single-mn_s)/NULLIF(mx_s-mn_s,0)*100,1) FROM g CROSS JOIN b;

-- ---------------------------------------------------------------------
-- 4. ads_platform_dist：下单平台分布（Flask 列 phone_type AS name）
-- ---------------------------------------------------------------------
DROP TABLE IF EXISTS ncs_ads.ads_platform_dist;
CREATE EXTERNAL TABLE ncs_ads.ads_platform_dist (
  phone_type STRING, user_count BIGINT
)
STORED AS PARQUET LOCATION '/user/hive/warehouse/ncs_ads.db/ads_platform_dist';

INSERT OVERWRITE TABLE ncs_ads.ads_platform_dist
SELECT CASE platform WHEN 'ios' THEN 'iOS'
                     WHEN 'android' THEN 'Android'
                     ELSE 'Web' END AS phone_type,
       COUNT(*) AS user_count
FROM ncs_dwd.dwd_charge_detail
GROUP BY platform;

-- ---------------------------------------------------------------------
-- 5. ads_hour_trend：24 小时充电量趋势，is_peak=高于平均小时的高峰
-- ---------------------------------------------------------------------
DROP TABLE IF EXISTS ncs_ads.ads_hour_trend;
CREATE EXTERNAL TABLE ncs_ads.ads_hour_trend (
  hour INT, sessions BIGINT, total_kwh DOUBLE, is_peak INT
)
STORED AS PARQUET LOCATION '/user/hive/warehouse/ncs_ads.db/ads_hour_trend';

INSERT OVERWRITE TABLE ncs_ads.ads_hour_trend
SELECT h.hour, h.sessions, h.total_kwh,
       CASE WHEN h.sessions >= a.avg_sess THEN 1 ELSE 0 END AS is_peak
FROM ncs_dws.dws_hour_agg h
CROSS JOIN (SELECT AVG(sessions) AS avg_sess FROM ncs_dws.dws_hour_agg) a;

-- ---------------------------------------------------------------------
-- 6. ads_station_type_eff：桩型效率（相对负载率 / 单桩均电量 / 平均电价）
-- ---------------------------------------------------------------------
DROP TABLE IF EXISTS ncs_ads.ads_station_type_eff;
CREATE EXTERNAL TABLE ncs_ads.ads_station_type_eff (
  gun_type STRING, utilization_rate DOUBLE, daily_kwh DOUBLE, avg_fee_per_kwh DOUBLE
)
STORED AS PARQUET LOCATION '/user/hive/warehouse/ncs_ads.db/ads_station_type_eff';

INSERT OVERWRITE TABLE ncs_ads.ads_station_type_eff
WITH t AS (
  SELECT station_type, COUNT(*) AS sess, SUM(kwh) AS kwh,
         SUM(total_fee) AS fee, SUM(device_count) AS guns
  FROM ncs_dwd.dwd_charge_detail GROUP BY station_type
), mx AS (SELECT MAX(sess) m FROM t)
SELECT t.station_type AS gun_type,
       ROUND(t.sess/mx.m*100,2)                          AS utilization_rate,
       ROUND(t.kwh/NULLIF(t.guns,0),2)                  AS daily_kwh,
       ROUND(t.fee/NULLIF(t.kwh,0),2)                   AS avg_fee_per_kwh
FROM t CROSS JOIN mx;

-- ---------------------------------------------------------------------
-- 7. ads_week_compare：工作日 vs 周末对比（含占比 %）
-- ---------------------------------------------------------------------
DROP TABLE IF EXISTS ncs_ads.ads_week_compare;
CREATE EXTERNAL TABLE ncs_ads.ads_week_compare (
  day_type STRING, sessions BIGINT, total_kwh DOUBLE, pct DOUBLE
)
STORED AS PARQUET LOCATION '/user/hive/warehouse/ncs_ads.db/ads_week_compare';

INSERT OVERWRITE TABLE ncs_ads.ads_week_compare
SELECT CASE is_weekend WHEN 1 THEN '周末' ELSE '工作日' END AS day_type,
       COUNT(*) AS sessions, ROUND(SUM(kwh),2) AS total_kwh,
       ROUND(COUNT(*)/SUM(COUNT(*)) OVER()*100,2) AS pct
FROM ncs_dwd.dwd_charge_detail
GROUP BY is_weekend;

-- ---------------------------------------------------------------------
-- 8. ads_battery_health：电池健康（起始 SOC 分段）占比
-- ---------------------------------------------------------------------
DROP TABLE IF EXISTS ncs_ads.ads_battery_health;
CREATE EXTERNAL TABLE ncs_ads.ads_battery_health (
  health_level STRING, sess_count BIGINT, ratio DOUBLE
)
STORED AS PARQUET LOCATION '/user/hive/warehouse/ncs_ads.db/ads_battery_health';

INSERT OVERWRITE TABLE ncs_ads.ads_battery_health
SELECT health_level, SUM(sess_count) AS sess_count,
       ROUND(SUM(sess_count)/SUM(SUM(sess_count)) OVER()*100,2) AS ratio
FROM ncs_dws.dws_bms_agg
GROUP BY health_level;

-- ---------------------------------------------------------------------
-- 9. ads_area_cost：区域（城市）营收/成本/利润
--    area 取 address 第一个逗号前的城市；成本单价常量 0.6 元/度（可调整）
-- ---------------------------------------------------------------------
DROP TABLE IF EXISTS ncs_ads.ads_area_cost;
CREATE EXTERNAL TABLE ncs_ads.ads_area_cost (
  station_area STRING, revenue DOUBLE, cost DOUBLE, profit DOUBLE, profit_rate DOUBLE
)
STORED AS PARQUET LOCATION '/user/hive/warehouse/ncs_ads.db/ads_area_cost';

INSERT OVERWRITE TABLE ncs_ads.ads_area_cost
SELECT COALESCE(NULLIF(substring_index(address,',',2),''),'未知区域') AS station_area,
       ROUND(SUM(total_fee),2)            AS revenue,
       ROUND(SUM(kwh)*0.6,2)              AS cost,
       ROUND(SUM(total_fee)-SUM(kwh)*0.6,2) AS profit,
       ROUND((SUM(total_fee)-SUM(kwh)*0.6)/NULLIF(SUM(total_fee),0)*100,2) AS profit_rate
FROM ncs_dwd.dwd_charge_detail
GROUP BY COALESCE(NULLIF(substring_index(address,',',2),''),'未知区域');

-- ---------------------------------------------------------------------
-- 10. ads_station_topn：站点 TOP10（相对负载率）
-- ---------------------------------------------------------------------
DROP TABLE IF EXISTS ncs_ads.ads_station_topn;
CREATE EXTERNAL TABLE ncs_ads.ads_station_topn (
  rn INT, station_name STRING, station_area STRING, total_sessions BIGINT,
  total_kwh DOUBLE, total_fee DOUBLE, utilization_rate DOUBLE
)
STORED AS PARQUET LOCATION '/user/hive/warehouse/ncs_ads.db/ads_station_topn';

INSERT OVERWRITE TABLE ncs_ads.ads_station_topn
SELECT CAST(rn AS INT), station_name, station_area, total_sessions,
       total_kwh, total_fee, utilization_rate
FROM (
  SELECT station_name,
         COALESCE(NULLIF(substring_index(address,',',2),''),'未知区域') AS station_area,
         total_sessions, total_kwh, total_fee,
         ROUND(total_sessions/MAX(total_sessions) OVER()*100,2) AS utilization_rate,
         ROW_NUMBER() OVER(ORDER BY total_sessions DESC, total_kwh DESC) AS rn
  FROM ncs_dws.dws_station_agg
) x
WHERE rn <= 10;

-- ------------------------- 全表结果核对（每张表都要有行） -------------------------
SELECT 'ads_kpi_overview'     AS tbl, COUNT(*) AS rows_cnt FROM ncs_ads.ads_kpi_overview
UNION ALL SELECT 'ads_user_level_dist', COUNT(*) FROM ncs_ads.ads_user_level_dist
UNION ALL SELECT 'ads_user_radar',       COUNT(*) FROM ncs_ads.ads_user_radar
UNION ALL SELECT 'ads_platform_dist',    COUNT(*) FROM ncs_ads.ads_platform_dist
UNION ALL SELECT 'ads_hour_trend',       COUNT(*) FROM ncs_ads.ads_hour_trend
UNION ALL SELECT 'ads_station_type_eff', COUNT(*) FROM ncs_ads.ads_station_type_eff
UNION ALL SELECT 'ads_week_compare',     COUNT(*) FROM ncs_ads.ads_week_compare
UNION ALL SELECT 'ads_battery_health',   COUNT(*) FROM ncs_ads.ads_battery_health
UNION ALL SELECT 'ads_area_cost',        COUNT(*) FROM ncs_ads.ads_area_cost
UNION ALL SELECT 'ads_station_topn',     COUNT(*) FROM ncs_ads.ads_station_topn;
