#!/usr/bin/env bash
set -Eeuo pipefail
source_dir="${1:?Usage: upload_raw.sh CSV_DIRECTORY}"
: "${RAW_HDFS_DIR:=/evcharging/raw}"
files=(orders/charging_orders.csv stations/charging_stations.csv piles/charging_piles.csv users/users.csv)
for file in "${files[@]}"; do
  test -f "${source_dir}/${file}" || { echo "Missing ${file}" >&2; exit 2; }
done
for file in "${files[@]}"; do
  hdfs dfs -mkdir -p "${RAW_HDFS_DIR}/$(dirname "${file}")"
  hdfs dfs -put -f "${source_dir}/${file}" "${RAW_HDFS_DIR}/${file}"
done
echo "Uploaded four CSV files to ${RAW_HDFS_DIR}"
