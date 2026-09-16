-- 功能：把 DWD/DWS 数据计算成大屏直接使用的十张 ADS 结果表。
-- 输入：ncs_dwd 质量/维度表、ncs_dws 汇总表和 ${COST_PER_KWH} 成本参数。
-- 输出/接口：KPI、用户、平台、时段、桩型、周类型、SOC、区域收益、站点排行十组结果。
-- 字段名称与顺序以 analytics/ads_contract.json 为准，由 evcharging_analysis.py 执行。
DROP TABLE IF EXISTS ncs_ads.ads_kpi_overview;
CREATE TABLE ncs_ads.ads_kpi_overview USING PARQUET AS
WITH totals AS (SELECT COUNT(*) AS sessions,CAST(COALESCE(SUM(kwh),0) AS DECIMAL(18,3)) AS total_kwh,
       CAST(COALESCE(SUM(total_fee),0) AS DECIMAL(18,2)) AS total_fee FROM ncs_dws.dws_sessions),
stations AS (SELECT COUNT(*) AS station_count FROM ncs_dwd.dwd_station)
SELECT t.sessions,t.total_kwh,t.total_fee,s.station_count,
       CAST(ROUND(q.rejected_count/q.raw_count*100,3) AS DECIMAL(8,3)) AS abnormal_rate
FROM totals t CROSS JOIN stations s CROSS JOIN ncs_dwd.dwd_quality q;
DROP TABLE IF EXISTS ncs_ads.ads_user_level_dist;
CREATE TABLE ncs_ads.ads_user_level_dist USING PARQUET AS
SELECT CASE WHEN charge_count>=10 THEN '高频用户' WHEN charge_count>=3 THEN '中频用户' ELSE '低频用户' END AS user_level,COUNT(*) AS user_count
FROM ncs_dws.dws_user_agg
GROUP BY CASE WHEN charge_count>=10 THEN '高频用户' WHEN charge_count>=3 THEN '中频用户' ELSE '低频用户' END;
DROP TABLE IF EXISTS ncs_ads.ads_user_radar;
CREATE TABLE ncs_ads.ads_user_radar USING PARQUET AS
WITH lv AS (
 SELECT CASE WHEN charge_count>=10 THEN '高频用户' WHEN charge_count>=3 THEN '中频用户' ELSE '低频用户' END AS user_level,
        charge_count,total_kwh,total_fee,avg_kwh FROM ncs_dws.dws_user_agg
), dims AS (
 SELECT user_level,'充电频次' AS dim_name,AVG(charge_count) AS v FROM lv GROUP BY user_level
 UNION ALL SELECT user_level,'累计电量',AVG(total_kwh) FROM lv GROUP BY user_level
 UNION ALL SELECT user_level,'消费金额',AVG(total_fee) FROM lv GROUP BY user_level
 UNION ALL SELECT user_level,'单次电量',AVG(avg_kwh) FROM lv GROUP BY user_level
)
SELECT user_level,dim_name,
 CAST(ROUND((v-MIN(v) OVER(PARTITION BY dim_name))/NULLIF(MAX(v) OVER(PARTITION BY dim_name)-MIN(v) OVER(PARTITION BY dim_name),0)*100,3) AS DECIMAL(8,3)) AS dim_value FROM dims;
DROP TABLE IF EXISTS ncs_ads.ads_platform_dist;
CREATE TABLE ncs_ads.ads_platform_dist USING PARQUET AS
SELECT platform AS phone_type,COUNT(DISTINCT user_id) AS user_count FROM ncs_dws.dws_sessions WHERE platform IS NOT NULL GROUP BY platform;
DROP TABLE IF EXISTS ncs_ads.ads_hour_trend;
CREATE TABLE ncs_ads.ads_hour_trend USING PARQUET AS
SELECT hour,sessions,total_kwh,CASE WHEN sessions>0 AND sessions>=AVG(sessions) OVER() THEN 1 ELSE 0 END AS is_peak FROM ncs_dws.dws_hour_agg;
DROP TABLE IF EXISTS ncs_ads.ads_station_type_eff;
CREATE TABLE ncs_ads.ads_station_type_eff USING PARQUET AS
WITH guns AS (SELECT station_type,COUNT(*) AS guns FROM ncs_dwd.dwd_pile GROUP BY station_type),
agg AS (SELECT station_type,COUNT(*) AS sessions,SUM(kwh) AS kwh,SUM(total_fee) AS fee FROM ncs_dws.dws_sessions GROUP BY station_type),
t AS (SELECT g.station_type,g.guns,COALESCE(u.sessions,0) AS sessions,COALESCE(u.kwh,0) AS kwh,COALESCE(u.fee,0) AS fee FROM guns g LEFT JOIN agg u ON g.station_type=u.station_type)
SELECT station_type AS gun_type,
 CAST(ROUND(COALESCE(sessions/NULLIF(MAX(sessions) OVER(),0)*100,0),3) AS DECIMAL(8,3)) AS utilization_rate,
 CAST(ROUND(kwh/guns/d.days,3) AS DECIMAL(18,3)) AS daily_kwh,
 CAST(ROUND(fee/NULLIF(kwh,0),4) AS DECIMAL(18,4)) AS avg_fee_per_kwh FROM t CROSS JOIN ncs_dws.dws_span d;
DROP TABLE IF EXISTS ncs_ads.ads_week_compare;
CREATE TABLE ncs_ads.ads_week_compare USING PARQUET AS
WITH types AS (SELECT 0 AS is_weekend,'工作日' AS day_type UNION ALL SELECT 1,'周末'),
agg AS (SELECT is_weekend,COUNT(*) AS sessions,SUM(kwh) AS total_kwh FROM ncs_dws.dws_sessions GROUP BY is_weekend),
t AS (SELECT ty.day_type,COALESCE(a.sessions,0) AS sessions,COALESCE(a.total_kwh,0) AS kwh FROM types ty LEFT JOIN agg a ON ty.is_weekend=a.is_weekend)
SELECT day_type,sessions,CAST(kwh AS DECIMAL(18,3)) AS total_kwh,
 CAST(ROUND(COALESCE(sessions/NULLIF(SUM(sessions) OVER(),0)*100,0),3) AS DECIMAL(8,3)) AS pct FROM t;
DROP TABLE IF EXISTS ncs_ads.ads_battery_health;
CREATE TABLE ncs_ads.ads_battery_health USING PARQUET AS
SELECT health_level,sess_count,CAST(ROUND(sess_count/NULLIF(SUM(sess_count) OVER(),0)*100,3) AS DECIMAL(8,3)) AS ratio FROM ncs_dws.dws_bms_agg;
DROP TABLE IF EXISTS ncs_ads.ads_area_cost;
CREATE TABLE ncs_ads.ads_area_cost USING PARQUET AS
SELECT station_area,CAST(ROUND(SUM(total_fee),2) AS DECIMAL(18,2)) AS revenue,
 CAST(ROUND(SUM(kwh)*${COST_PER_KWH},2) AS DECIMAL(18,2)) AS cost,
 CAST(ROUND(SUM(total_fee)-SUM(kwh)*${COST_PER_KWH},2) AS DECIMAL(18,2)) AS profit,
 CAST(ROUND((SUM(total_fee)-SUM(kwh)*${COST_PER_KWH})/NULLIF(SUM(total_fee),0)*100,3) AS DECIMAL(8,3)) AS profit_rate
FROM ncs_dws.dws_sessions GROUP BY station_area;
DROP TABLE IF EXISTS ncs_ads.ads_station_topn;
CREATE TABLE ncs_ads.ads_station_topn USING PARQUET AS
SELECT CAST(rn AS INT) AS rn,station_name,station_area,total_sessions,total_kwh,total_fee,utilization_rate
FROM (SELECT station_name,station_area,total_sessions,total_kwh,total_fee,
 CAST(ROUND(COALESCE(total_sessions/NULLIF(MAX(total_sessions) OVER(),0)*100,0),3) AS DECIMAL(8,3)) AS utilization_rate,
 ROW_NUMBER() OVER(ORDER BY total_sessions DESC,total_kwh DESC,station_id ASC) AS rn FROM ncs_dws.dws_station_agg) t WHERE rn<=10;
