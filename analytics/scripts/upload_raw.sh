#!/usr/bin/env bash
# 功能：把四类脱敏业务 CSV 上传到 HDFS 原始层。
# 输入：第一个参数是 CSV 根目录，RAW_HDFS_DIR 可覆盖默认 /evcharging/raw。
# 输出/接口：在 HDFS 创建目录并覆盖对应原始文件；任一文件缺失就停止。
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
