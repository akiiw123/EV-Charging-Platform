#!/usr/bin/env bash
# 功能：安全加载私有 .env，并使用 Gunicorn 启动 Flask 分析 API。
# 输入：ANALYTICS_ENV_FILE 或 analytics/.env，以及可选 ANALYTICS_PYTHON。
# 输出/接口：默认监听 8091，提供 /api/v1/* 和构建后的 /dashboard/。
set -Eeuo pipefail
analytics_dir="$(cd "$(dirname "$0")/.." && pwd)"
cd "${analytics_dir}"
env_file="${ANALYTICS_ENV_FILE:-${analytics_dir}/.env}"
test -f "${env_file}" || { echo "Create private .env from .env.example first" >&2; exit 2; }
set -a
source "${env_file}"
set +a
exec "${analytics_dir}/.venv/bin/gunicorn" -c deploy/gunicorn.conf.py wsgi:app
