CREATE TABLE IF NOT EXISTS etl_batches (
    batch_id VARCHAR(32) PRIMARY KEY,
    source_path VARCHAR(512) NOT NULL,
    status ENUM('running', 'success', 'failed') NOT NULL,
    started_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    finished_at DATETIME NULL,
    detail VARCHAR(1000) NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='ADS导入批次';

CREATE TABLE IF NOT EXISTS ads_kpi_overview (
    sessions BIGINT NOT NULL,
    total_kwh DECIMAL(18,3) NOT NULL,
    total_fee DECIMAL(18,2) NOT NULL,
    station_count BIGINT NOT NULL,
    abnormal_rate DECIMAL(8,3) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='KPI总览';

CREATE TABLE IF NOT EXISTS ads_user_level_dist (
    user_level VARCHAR(20) PRIMARY KEY,
    user_count BIGINT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户等级分布';

CREATE TABLE IF NOT EXISTS ads_user_radar (
    user_level VARCHAR(20) NOT NULL,
    dim_name VARCHAR(20) NOT NULL,
    dim_value DECIMAL(8,3) NOT NULL,
    PRIMARY KEY (user_level, dim_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户行为雷达';

CREATE TABLE IF NOT EXISTS ads_platform_dist (
    phone_type VARCHAR(20) PRIMARY KEY,
    user_count BIGINT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='平台偏好分布';

CREATE TABLE IF NOT EXISTS ads_hour_trend (
    hour TINYINT PRIMARY KEY,
    sessions BIGINT NOT NULL,
    total_kwh DECIMAL(18,3) NOT NULL,
    is_peak TINYINT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='24小时充电趋势';

CREATE TABLE IF NOT EXISTS ads_station_type_eff (
    gun_type VARCHAR(20) PRIMARY KEY,
    utilization_rate DECIMAL(8,3) NOT NULL,
    daily_kwh DECIMAL(18,3) NOT NULL,
    avg_fee_per_kwh DECIMAL(18,4) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='站点类型运营效率';

CREATE TABLE IF NOT EXISTS ads_week_compare (
    day_type VARCHAR(10) PRIMARY KEY,
    sessions BIGINT NOT NULL,
    total_kwh DECIMAL(18,3) NOT NULL,
    pct DECIMAL(8,3) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='工作日周末对比';

CREATE TABLE IF NOT EXISTS ads_battery_health (
    health_level VARCHAR(30) PRIMARY KEY,
    sess_count BIGINT NOT NULL,
    ratio DECIMAL(8,3) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='起始电量等级分布';

CREATE TABLE IF NOT EXISTS ads_area_cost (
    station_area VARCHAR(50) PRIMARY KEY,
    revenue DECIMAL(18,2) NOT NULL,
    cost DECIMAL(18,2) NOT NULL,
    profit DECIMAL(18,2) NOT NULL,
    profit_rate DECIMAL(8,3) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='区域成本收益';

CREATE TABLE IF NOT EXISTS ads_station_topn (
    rn INT PRIMARY KEY,
    station_name VARCHAR(100) NOT NULL,
    station_area VARCHAR(50) NOT NULL,
    total_sessions BIGINT NOT NULL,
    total_kwh DECIMAL(18,3) NOT NULL,
    total_fee DECIMAL(18,2) NOT NULL,
    utilization_rate DECIMAL(8,3) NOT NULL,
    INDEX idx_station_topn_name (station_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='运营效率TOP充电站';
