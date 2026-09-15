#!/usr/bin/env bash
set -Eeuo pipefail
analytics_dir="$(cd "$(dirname "$0")/.." && pwd)"
cd "${analytics_dir}"
env_file="${ANALYTICS_ENV_FILE:-${analytics_dir}/.env}"
test -f "${env_file}" || { echo "Create private .env from .env.example first" >&2; exit 2; }
set -a
source "${env_file}"
set +a
exec "${analytics_dir}/.venv/bin/gunicorn" -c deploy/gunicorn.conf.py wsgi:app
