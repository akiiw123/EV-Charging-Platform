CREATE TABLE IF NOT EXISTS schema_versions (
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    phone TEXT NOT NULL UNIQUE CHECK(length(phone) = 11),
    nickname TEXT NOT NULL,
    avatar_path TEXT,
    wallet_balance REAL NOT NULL DEFAULT 0 CHECK(wallet_balance >= 0),
    status TEXT NOT NULL DEFAULT 'active' CHECK(status IN ('active', 'frozen')),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS administrators (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    must_change_password INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS charging_stations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    address TEXT NOT NULL,
    latitude REAL NOT NULL,
    longitude REAL NOT NULL,
    price_per_kwh REAL NOT NULL CHECK(price_per_kwh >= 0),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS charging_piles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id INTEGER NOT NULL REFERENCES charging_stations(id) ON DELETE CASCADE,
    code TEXT NOT NULL UNIQUE,
    type TEXT NOT NULL CHECK(type IN ('fast', 'slow')),
    power_kw REAL NOT NULL CHECK(power_kw > 0),
    status TEXT NOT NULL DEFAULT 'idle' CHECK(status IN ('idle', 'charging', 'fault', 'offline')),
    charge_count INTEGER NOT NULL DEFAULT 0,
    total_charge_minutes INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS charging_orders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES users(id),
    pile_id INTEGER NOT NULL REFERENCES charging_piles(id),
    status TEXT NOT NULL CHECK(status IN ('reserved', 'charging', 'awaiting_payment', 'completed', 'cancelled')),
    started_at TEXT,
    ended_at TEXT,
    energy_kwh REAL NOT NULL DEFAULT 0 CHECK(energy_kwh >= 0),
    amount REAL NOT NULL DEFAULT 0 CHECK(amount >= 0),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS recharge_records (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES users(id),
    amount REAL NOT NULL CHECK(amount > 0),
    balance_before REAL NOT NULL DEFAULT 0 CHECK(balance_before >= 0),
    balance_after REAL NOT NULL DEFAULT 0 CHECK(balance_after >= 0),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_user_active_order
ON charging_orders(user_id)
WHERE status IN ('reserved', 'charging', 'awaiting_payment');

CREATE UNIQUE INDEX IF NOT EXISTS idx_pile_active_order
ON charging_orders(pile_id)
WHERE status IN ('reserved', 'charging', 'awaiting_payment');

-- 站点收费规则（每站最多一条）。
-- enabled 只控制分时电价：未建立规则或 enabled = 0 时，电价回退到
-- charging_stations.price_per_kwh，保证既有充电计费流程不受本次新增表影响。
-- 免费挪车时间与占位费不受 enabled 影响，只要规则行存在即生效；站点无规则则不收取占位费。
-- occupancy_fee_cap <= 0 表示占位费不封顶。
CREATE TABLE IF NOT EXISTS charging_pricing_rules (
    station_id INTEGER PRIMARY KEY REFERENCES charging_stations(id) ON DELETE CASCADE,
    enabled INTEGER NOT NULL DEFAULT 1 CHECK(enabled IN (0, 1)),
    free_move_minutes INTEGER NOT NULL DEFAULT 0 CHECK(free_move_minutes >= 0),
    occupancy_fee_per_minute REAL NOT NULL DEFAULT 0 CHECK(occupancy_fee_per_minute >= 0),
    occupancy_fee_cap REAL NOT NULL DEFAULT 0 CHECK(occupancy_fee_cap >= 0),
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- 分时电价段。start_minute / end_minute 为当日 00:00 起的分钟数，区间左闭右开，
-- 取值范围 [0, 1440]；跨零点时段拆成两条记录（例如 22:00-24:00 与 00:00-08:00）。
CREATE TABLE IF NOT EXISTS charging_pricing_periods (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id INTEGER NOT NULL REFERENCES charging_stations(id) ON DELETE CASCADE,
    start_minute INTEGER NOT NULL CHECK(start_minute >= 0 AND start_minute < 1440),
    end_minute INTEGER NOT NULL CHECK(end_minute > 0 AND end_minute <= 1440),
    period_type TEXT NOT NULL CHECK(period_type IN ('peak', 'flat', 'valley')),
    price_per_kwh REAL NOT NULL CHECK(price_per_kwh >= 0),
    CHECK(start_minute < end_minute),
    UNIQUE(station_id, start_minute)
);

CREATE INDEX IF NOT EXISTS idx_pricing_periods_station
ON charging_pricing_periods(station_id, start_minute);

INSERT OR IGNORE INTO schema_versions(version) VALUES (1);
INSERT OR IGNORE INTO schema_versions(version) VALUES (2);
INSERT OR IGNORE INTO schema_versions(version) VALUES (3);
INSERT OR IGNORE INTO schema_versions(version) VALUES (4);