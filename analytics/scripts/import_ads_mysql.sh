#!/usr/bin/env bash
# 功能：为 MySQL 导入器提供简短、固定的 Shell 启动入口。
# 输入：原样转交 --dir、--validate-only 等参数，并可用 ANALYTICS_PYTHON 指定解释器。
# 输出/接口：调用 import_ads_mysql.py，退出码与 Python 校验/导入结果一致。
set -Eeuo pipefail
script_dir="$(cd "$(dirname "$0")" && pwd)"
: "${ANALYTICS_PYTHON:=${script_dir}/../.venv/bin/python}"
"${ANALYTICS_PYTHON}" "${script_dir}/import_ads_mysql.py" "$@"
