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

CREATE UNIQUE INDEX IF NOT EXISTS idx_user_active_order
ON charging_orders(user_id)
WHERE status IN ('reserved', 'charging', 'awaiting_payment');

INSERT OR IGNORE INTO schema_versions(version) VALUES (1);
