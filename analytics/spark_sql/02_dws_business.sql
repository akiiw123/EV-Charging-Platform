-- Statistics use charging/awaiting_payment/completed; revenue only completed.
DROP TABLE IF EXISTS ncs_dws.dws_sessions;
CREATE TABLE ncs_dws.dws_sessions USING PARQUET AS
SELECT * FROM ncs_dwd.dwd_charge_detail WHERE status IN ('charging','awaiting_payment','completed');
DROP TABLE IF EXISTS ncs_dws.dws_span;
CREATE TABLE ncs_dws.dws_span USING PARQUET AS
SELECT GREATEST(COALESCE(DATEDIFF(MAX(charge_date),MIN(charge_date))+1,1),1) AS days FROM ncs_dws.dws_sessions;
DROP TABLE IF EXISTS ncs_dws.dws_user_agg;
CREATE TABLE ncs_dws.dws_user_agg USING PARQUET AS
SELECT u.id AS user_id,COUNT(s.id) AS charge_count,COALESCE(SUM(s.kwh),0) AS total_kwh,
       COALESCE(SUM(s.total_fee),0) AS total_fee,COALESCE(AVG(s.kwh),0) AS avg_kwh
FROM ncs_dwd.dwd_user u LEFT JOIN ncs_dws.dws_sessions s ON u.id=s.user_id GROUP BY u.id;
DROP TABLE IF EXISTS ncs_dws.dws_hour_agg;
CREATE TABLE ncs_dws.dws_hour_agg USING PARQUET AS
SELECT CAST(h.id AS INT) AS hour,COUNT(s.id) AS sessions,CAST(COALESCE(SUM(s.kwh),0) AS DECIMAL(18,3)) AS total_kwh
FROM range(24) h LEFT JOIN ncs_dws.dws_sessions s ON h.id=s.hour GROUP BY h.id;
DROP TABLE IF EXISTS ncs_dws.dws_station_agg;
CREATE TABLE ncs_dws.dws_station_agg USING PARQUET AS
SELECT st.id AS station_id,st.station_name,st.station_area,COUNT(s.id) AS total_sessions,
       CAST(COALESCE(SUM(s.kwh),0) AS DECIMAL(18,3)) AS total_kwh,
       CAST(COALESCE(SUM(s.total_fee),0) AS DECIMAL(18,2)) AS total_fee
FROM ncs_dwd.dwd_station st LEFT JOIN ncs_dws.dws_sessions s ON st.id=s.station_id
GROUP BY st.id,st.station_name,st.station_area;
DROP TABLE IF EXISTS ncs_dws.dws_bms_agg;
CREATE TABLE ncs_dws.dws_bms_agg USING PARQUET AS
SELECT CASE WHEN soc<20 THEN '低电量(<20%)' WHEN soc<80 THEN '中电量(20-80%)' ELSE '高电量(>=80%)' END AS health_level,COUNT(*) AS sess_count
FROM ncs_dws.dws_sessions WHERE soc IS NOT NULL
GROUP BY CASE WHEN soc<20 THEN '低电量(<20%)' WHEN soc<80 THEN '中电量(20-80%)' ELSE '高电量(>=80%)' END;
