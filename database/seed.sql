-- 开发演示数据。管理员密码目前为课程说明书中的明文占位；认证功能完成时必须替换为加盐哈希。
INSERT OR IGNORE INTO administrators(username, password_hash)
VALUES ('admin', 'DEV_ONLY:123456');

INSERT OR IGNORE INTO charging_stations(id, name, address, latitude, longitude, price_per_kwh)
VALUES (1, '软件园充电站', '示例地址：软件园A区', 39.9042, 116.4074, 1.20);

INSERT OR IGNORE INTO charging_piles(station_id, code, type, power_kw, status)
VALUES
    (1, 'PILE-001', 'fast', 120, 'idle'),
    (1, 'PILE-002', 'slow', 7, 'idle');
