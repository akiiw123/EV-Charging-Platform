#!/usr/bin/env bash
set -Eeuo pipefail
script_dir="$(cd "$(dirname "$0")" && pwd)"
: "${ANALYTICS_PYTHON:=${script_dir}/../.venv/bin/python}"
"${ANALYTICS_PYTHON}" "${script_dir}/import_ads_mysql.py" "$@"
