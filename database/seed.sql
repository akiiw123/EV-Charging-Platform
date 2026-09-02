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
