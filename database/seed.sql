-- 开发演示数据。管理员密码目前为课程说明书中的明文占位；认证功能完成时必须替换为加盐哈希。
INSERT OR IGNORE INTO administrators(username, password_hash)
VALUES ('admin', 'DEV_ONLY:123456');

INSERT OR IGNORE INTO charging_stations(id, name, address, latitude, longitude, price_per_kwh)
VALUES (1, '软件园充电站', '示例地址：软件园A区', 39.9042, 116.4074, 1.20);

INSERT OR IGNORE INTO charging_piles(station_id, code, type, power_kw, status)
VALUES
    (1, 'PILE-001', 'fast', 120, 'idle'),
    (1, 'PILE-002', 'slow', 7, 'idle');

-- 用户端固定演示账号；仅在记录不存在时创建，不覆盖运行中产生的数据。
INSERT OR IGNORE INTO users(id, phone, nickname, wallet_balance, status)
VALUES
    (900001, '18800000001', '余额充足用户', 200.00, 'active'),
    (900002, '18800000002', '待结算用户', 80.00, 'active'),
    (900003, '18800000003', '低余额用户', 0.50, 'active'),
    (900004, '18800000004', '冻结用户', 50.00, 'frozen');

INSERT OR IGNORE INTO charging_stations(id, name, address, latitude, longitude, price_per_kwh)
VALUES (900001, '深圳演示充电站', '深圳市福田区市民中心', 22.543099, 114.057868, 1.35);

INSERT OR IGNORE INTO charging_piles(id, station_id, code, type, power_kw, status)
VALUES
    (900001, 900001, 'SZ001-01', 'fast', 120, 'idle'),
    (900002, 900001, 'SZ001-02', 'slow', 7, 'idle');

INSERT OR IGNORE INTO charging_orders(id, user_id, pile_id, status, started_at, ended_at,
                                      energy_kwh, amount, created_at)
VALUES (900001, 900002, 900002, 'awaiting_payment',
        datetime('now','-2 hours'), datetime('now','-1 hour'), 7.0, 9.45,
        datetime('now','-2 hours'));

UPDATE charging_piles SET status = 'fault'
WHERE id = 900002 AND code = 'SZ001-02';

-- ===== 良乡校区周边演示电站(PR#6 新增) =====
-- 北京理工大学良乡校区周边模拟充电站
-- 以下数据仅用于课程项目功能演示

INSERT OR IGNORE INTO charging_stations
(id, name, address, latitude, longitude, price_per_kwh)
VALUES
    (900010, '北理工良乡东门充电站',
     '北京市房山区北京理工大学良乡校区东门附近',
     39.7308, 116.1715, 1.28),

    (900011, '良乡大学城快充站',
     '北京市房山区良乡大学城地铁站附近',
     39.7278, 116.1692, 1.32),

    (900012, '北理工良乡南门充电站',
     '北京市房山区北京理工大学良乡校区南门附近',
     39.7249, 116.1728, 1.25),

    (900013, '良乡高教园充电站',
     '北京市房山区良乡高教园区',
     39.7332, 116.1648, 1.38),

    (900014, '学园北街充电站',
     '北京市房山区良乡大学城学园北街附近',
     39.7350, 116.1760, 1.30);


-- 为北京模拟充电站添加充电桩
INSERT OR IGNORE INTO charging_piles
(id, station_id, code, type, power_kw, status)
VALUES
    (900010, 900010, 'BJ010-01', 'fast', 120, 'idle'),
    (900011, 900010, 'BJ010-02', 'slow', 7, 'idle'),

    (900012, 900011, 'BJ011-01', 'fast', 120, 'idle'),
    (900013, 900011, 'BJ011-02', 'fast', 60, 'idle'),

    (900014, 900012, 'BJ012-01', 'fast', 120, 'idle'),
    (900015, 900012, 'BJ012-02', 'slow', 7, 'idle'),

    (900016, 900013, 'BJ013-01', 'fast', 180, 'idle'),
    (900017, 900013, 'BJ013-02', 'fast', 120, 'idle'),

    (900018, 900014, 'BJ014-01', 'fast', 120, 'idle'),
    (900019, 900014, 'BJ014-02', 'slow', 7, 'idle');

-- 收费规则演示数据：1 号站配置完整分时电价 + 占位费。
-- 时段为左闭右开的当日分钟区间，跨零点拆成两条。
INSERT OR IGNORE INTO charging_pricing_rules
    (station_id, enabled, free_move_minutes, occupancy_fee_per_minute, occupancy_fee_cap)
VALUES (1, 1, 15, 0.50, 30.00);

INSERT OR IGNORE INTO charging_pricing_periods
    (station_id, start_minute, end_minute, period_type, price_per_kwh)
VALUES
    (1,    0,  480, 'valley', 0.80),
    (1,  480,  660, 'peak',   1.50),
    (1,  660, 1080, 'flat',   1.20),
    (1, 1080, 1320, 'peak',   1.50),
    (1, 1320, 1440, 'valley', 0.80);

-- 900001 号站只配置占位费，不配置分时电价段，
-- 用于演示"查不到时段则回退 charging_stations.price_per_kwh"。
INSERT OR IGNORE INTO charging_pricing_rules
    (station_id, enabled, free_move_minutes, occupancy_fee_per_minute, occupancy_fee_cap)
VALUES (900001, 1, 10, 0.80, 0);
