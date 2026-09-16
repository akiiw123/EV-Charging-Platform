#!/usr/bin/env bash
# 功能：在 Spark 主机上一键运行分析并导出十张 ADS 文件。
# 输入：ADS_EXPORT_DIR、SPARK_SUBMIT、RAW_HDFS_DIR 等环境变量。
# 输出/接口：调用 export_ads.py --analyze，在目标目录生成 CSV 和 manifest.json。
set -Eeuo pipefail
script_dir="$(cd "$(dirname "$0")" && pwd)"
cd "${script_dir}/.."
: "${ADS_EXPORT_DIR:=${script_dir}/../runtime/ads_export}"
: "${SPARK_SUBMIT:=spark-submit}"
: "${PYSPARK_PYTHON:=${script_dir}/../.venv/bin/python}"
: "${PYSPARK_DRIVER_PYTHON:=${PYSPARK_PYTHON}}"
export PYSPARK_PYTHON PYSPARK_DRIVER_PYTHON
mkdir -p "${script_dir}/../runtime"
exec 9>"${script_dir}/../runtime/etl.lock"
flock -n 9 || { echo "Analysis/export already running" >&2; exit 1; }
"${SPARK_SUBMIT}" --master "${SPARK_MASTER:-local[*]}" "${script_dir}/export_ads.py" --out "${ADS_EXPORT_DIR}" "$@"
