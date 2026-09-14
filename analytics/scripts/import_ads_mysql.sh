#!/usr/bin/env bash
set -Eeuo pipefail

: "${MYSQL_HOST:=127.0.0.1}"
: "${MYSQL_PORT:=3306}"
: "${MYSQL_USER:?MYSQL_USER is required}"
: "${MYSQL_PASSWORD:=}"
: "${MYSQL_DATABASE:=charging_ads}"
: "${ADS_EXPORT_DIR:?ADS_EXPORT_DIR is required}"

case "${MYSQL_DATABASE}" in
  *[!A-Za-z0-9_]*|'') echo "MYSQL_DATABASE may contain only letters, digits, and underscores" >&2; exit 2 ;;
esac

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
SCHEMA_FILE=$(cd "${SCRIPT_DIR}/../sql" && pwd)/mysql_schema.sql
EXPORT_DIR=$(cd "${ADS_EXPORT_DIR}" && pwd)
BATCH_ID=$(date +%Y%m%d%H%M%S)

case "${EXPORT_DIR}" in
  *"'"*|*$'\n'*) echo "ADS_EXPORT_DIR contains unsupported characters" >&2; exit 2 ;;
esac

TABLES=(
  ads_kpi_overview
  ads_user_level_dist
  ads_user_radar
  ads_platform_dist
  ads_hour_trend
  ads_station_type_eff
  ads_week_compare
  ads_battery_health
  ads_area_cost
  ads_station_topn
)

for table in "${TABLES[@]}"; do
  test -f "${EXPORT_DIR}/${table}.csv" || {
    echo "Missing export file: ${EXPORT_DIR}/${table}.csv" >&2
    exit 2
  }
done

if [ -n "${MYSQL_PASSWORD}" ]; then
  export MYSQL_PWD="${MYSQL_PASSWORD}"
else
  unset MYSQL_PWD || true
fi
MYSQL=(mysql --protocol=tcp --local-infile=1 --default-character-set=utf8mb4 \
  -h "${MYSQL_HOST}" -P "${MYSQL_PORT}" -u "${MYSQL_USER}")

"${MYSQL[@]}" "${MYSQL_DATABASE}" < "${SCHEMA_FILE}"
"${MYSQL[@]}" "${MYSQL_DATABASE}" -e \
  "INSERT INTO etl_batches(batch_id,source_path,status) VALUES('${BATCH_ID}','${EXPORT_DIR}','running')"

mark_failed() {
  "${MYSQL[@]}" "${MYSQL_DATABASE}" -e \
    "UPDATE etl_batches SET status='failed',finished_at=NOW(),detail='import failed' WHERE batch_id='${BATCH_ID}'" \
    >/dev/null 2>&1 || true
}
trap mark_failed ERR

for table in "${TABLES[@]}"; do
  file="${EXPORT_DIR}/${table}.csv"
  echo "Importing ${table} from ${file}"
  "${MYSQL[@]}" "${MYSQL_DATABASE}" <<SQL
CREATE TEMPORARY TABLE stage LIKE ${table};
LOAD DATA LOCAL INFILE '${file}'
INTO TABLE stage
CHARACTER SET utf8mb4
FIELDS TERMINATED BY '|'
LINES TERMINATED BY '\n';
START TRANSACTION;
DELETE FROM ${table};
INSERT INTO ${table} SELECT * FROM stage;
COMMIT;
SELECT '${table}' AS table_name, COUNT(*) AS row_count FROM ${table};
SQL
done

trap - ERR
"${MYSQL[@]}" "${MYSQL_DATABASE}" -e \
  "UPDATE etl_batches SET status='success',finished_at=NOW(),detail='10 tables imported' WHERE batch_id='${BATCH_ID}'"
echo "ADS import completed: batch=${BATCH_ID}"
