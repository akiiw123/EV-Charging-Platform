#!/usr/bin/env bash
set -Eeuo pipefail

: "${SPARK_SQL:=spark-sql}"
: "${ADS_DATABASE:=ncs_ads}"
: "${ADS_EXPORT_DIR:=/home/hadoop/temp/charging_ads_export}"

case "${ADS_DATABASE}" in
  *[!A-Za-z0-9_]*|'') echo "ADS_DATABASE may contain only letters, digits, and underscores" >&2; exit 2 ;;
esac
case "${ADS_EXPORT_DIR}" in
  /home/hadoop/temp/*) ;;
  *) echo "ADS_EXPORT_DIR must be below /home/hadoop/temp" >&2; exit 2 ;;
esac
case "${ADS_EXPORT_DIR}" in
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

mkdir -p /home/hadoop/temp
WORK_DIR=$(mktemp -d /home/hadoop/temp/charging_ads_export.tmp.XXXXXX)
cleanup() {
  if test -d "${WORK_DIR}"; then
    rm -rf -- "${WORK_DIR}"
  fi
}
trap cleanup EXIT

SQL_FILE="${WORK_DIR}/export.sql"
printf '%s\n' "SET hive.exec.compress.output=false;" > "${SQL_FILE}"
for table in "${TABLES[@]}"; do
  table_dir="${WORK_DIR}/${table}.parts"
  printf '%s\n' \
    "INSERT OVERWRITE LOCAL DIRECTORY '${table_dir}'" \
    "ROW FORMAT DELIMITED" \
    "FIELDS TERMINATED BY '|'" \
    "LINES TERMINATED BY '\\n'" \
    "NULL DEFINED AS ''" \
    "SELECT * FROM ${ADS_DATABASE}.${table};" >> "${SQL_FILE}"
done

echo "Exporting 10 ADS tables in one Spark SQL session"
"${SPARK_SQL}" -S \
  --conf spark.sql.shuffle.partitions=20 \
  -f "${SQL_FILE}"

for table in "${TABLES[@]}"; do
  table_dir="${WORK_DIR}/${table}.parts"
  output_file="${WORK_DIR}/${table}.csv"
  if ! test -d "${table_dir}"; then
    echo "Spark did not create the export directory for ${table}" >&2
    exit 1
  fi
  parts=()
  while IFS= read -r part; do
    parts+=("${part}")
  done < <(find "${table_dir}" -maxdepth 1 -type f -name 'part-*' -print | sort)
  if [ "${#parts[@]}" -eq 0 ]; then
    echo "No Spark part files produced for ${table}" >&2
    exit 1
  fi
  cat "${parts[@]}" > "${output_file}"
  rm -rf -- "${table_dir}"
  echo "  rows=$(wc -l < "${output_file}")"
done
rm -f -- "${SQL_FILE}"

if test -e "${ADS_EXPORT_DIR}"; then
  BACKUP_DIR="${ADS_EXPORT_DIR}.previous.$(date +%Y%m%d%H%M%S)"
  mv -- "${ADS_EXPORT_DIR}" "${BACKUP_DIR}"
  echo "Previous export moved to ${BACKUP_DIR}"
fi
mv -- "${WORK_DIR}" "${ADS_EXPORT_DIR}"
trap - EXIT
echo "ADS export completed: ${ADS_EXPORT_DIR}"
