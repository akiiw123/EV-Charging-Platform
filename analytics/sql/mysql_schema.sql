-- 功能：创建 MySQL 分析结果库所需的批次表、元数据表和十张 ADS 表。
-- 输入：由具备建表权限的管理员执行；表结构必须与 analytics/ads_contract.json 一致。
-- 输出/接口：Flask 使用只读账号查询，ETL 使用写账号按批次原子替换结果。

CREATE TABLE IF NOT EXISTS etl_batches (
batch_id VARCHAR(32) PRIMARY KEY, source_path VARCHAR(512) NOT NULL,
status ENUM('running','success','failed') NOT NULL,
started_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, finished_at DATETIME NULL,
detail VARCHAR(1000) NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS etl_metadata (
singleton TINYINT PRIMARY KEY, batch_id VARCHAR(32) NOT NULL, payload LONGTEXT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


CREATE TABLE IF NOT EXISTS ads_kpi_overview (
    sessions BIGINT NOT NULL,
    total_kwh DECIMAL(18,3) NOT NULL,
    total_fee DECIMAL(18,2) NOT NULL,
    station_count BIGINT NOT NULL,
    abnormal_rate DECIMAL(8,3) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS ads_user_level_dist (
    user_level VARCHAR(20) NOT NULL,
    user_count BIGINT NOT NULL,
    PRIMARY KEY (user_level)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS ads_user_radar (
    user_level VARCHAR(20) NOT NULL,
    dim_name VARCHAR(20) NOT NULL,
    dim_value DECIMAL(8,3) NULL,
    PRIMARY KEY (user_level,dim_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS ads_platform_dist (
    phone_type VARCHAR(20) NOT NULL,
    user_count BIGINT NOT NULL,
    PRIMARY KEY (phone_type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS ads_hour_trend (
    hour TINYINT NOT NULL,
    sessions BIGINT NOT NULL,
    total_kwh DECIMAL(18,3) NOT NULL,
    is_peak TINYINT NOT NULL,
    PRIMARY KEY (hour)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS ads_station_type_eff (
    gun_type VARCHAR(20) NOT NULL,
    utilization_rate DECIMAL(8,3) NOT NULL,
    daily_kwh DECIMAL(18,3) NULL,
    avg_fee_per_kwh DECIMAL(18,4) NULL,
    PRIMARY KEY (gun_type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS ads_week_compare (
    day_type VARCHAR(10) NOT NULL,
    sessions BIGINT NOT NULL,
    total_kwh DECIMAL(18,3) NOT NULL,
    pct DECIMAL(8,3) NOT NULL,
    PRIMARY KEY (day_type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS ads_battery_health (
    health_level VARCHAR(30) NOT NULL,
    sess_count BIGINT NOT NULL,
    ratio DECIMAL(8,3) NOT NULL,
    PRIMARY KEY (health_level)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS ads_area_cost (
    station_area VARCHAR(50) NOT NULL,
    revenue DECIMAL(18,2) NOT NULL,
    cost DECIMAL(18,2) NOT NULL,
    profit DECIMAL(18,2) NOT NULL,
    profit_rate DECIMAL(8,3) NULL,
    PRIMARY KEY (station_area)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS ads_station_topn (
    rn INT NOT NULL,
    station_name VARCHAR(100) NOT NULL,
    station_area VARCHAR(50) NOT NULL,
    total_sessions BIGINT NOT NULL,
    total_kwh DECIMAL(18,3) NOT NULL,
    total_fee DECIMAL(18,2) NOT NULL,
    utilization_rate DECIMAL(8,3) NOT NULL,
    PRIMARY KEY (rn)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

ALTER TABLE ads_user_radar MODIFY COLUMN dim_value DECIMAL(8,3) NULL;

ALTER TABLE ads_station_type_eff MODIFY COLUMN daily_kwh DECIMAL(18,3) NULL;

ALTER TABLE ads_station_type_eff MODIFY COLUMN avg_fee_per_kwh DECIMAL(18,4) NULL;

ALTER TABLE ads_area_cost MODIFY COLUMN profit_rate DECIMAL(8,3) NULL;
