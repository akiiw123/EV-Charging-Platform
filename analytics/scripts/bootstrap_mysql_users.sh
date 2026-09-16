#!/usr/bin/env bash
# 功能：创建/升级 charging_ads 表结构，并建立 ETL 写账号与 API 只读账号。
# 输入：MYSQL_* 环境变量；需要时在终端隐藏读取管理员密码。
# 输出/接口：执行 sql/mysql_schema.sql，并把随机业务密码写入仓库外的私有环境文件。
set -Eeuo pipefail
umask 077

: "${MYSQL_DATABASE:=charging_ads}"
: "${MYSQL_PORT:=3306}"
: "${MYSQL_ADMIN_USER:=root}"
: "${MYSQL_API_HOST:=127.0.0.1}"

case "${MYSQL_DATABASE}" in
  *[!A-Za-z0-9_]*|'') echo "MYSQL_DATABASE may contain only letters, digits, and underscores" >&2; exit 2 ;;
esac

command -v mysql >/dev/null 2>&1 || { echo "mysql client is required" >&2; exit 1; }
command -v openssl >/dev/null 2>&1 || { echo "openssl is required" >&2; exit 1; }

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
: "${SECRETS_DIR:=${SCRIPT_DIR}/../.secrets}"
SCHEMA_FILE=$(cd "${SCRIPT_DIR}/../sql" && pwd)/mysql_schema.sql
mkdir -p "${SECRETS_DIR}"
chmod 700 "${SECRETS_DIR}"

MYSQL_ADMIN=(mysql -u "${MYSQL_ADMIN_USER}")
if ! "${MYSQL_ADMIN[@]}" -N -e "SELECT 1" >/dev/null 2>&1; then
  if [ ! -t 0 ]; then
    echo "MySQL admin authentication is required; run this script in an interactive terminal" >&2
    exit 1
  fi
  read -r -s -p "MySQL admin password for ${MYSQL_ADMIN_USER}: " admin_password
  echo
  export MYSQL_PWD="${admin_password}"
  "${MYSQL_ADMIN[@]}" -N -e "SELECT 1" >/dev/null
fi

API_ENV="${SECRETS_DIR}/analytics-api.env"
ETL_ENV="${SECRETS_DIR}/analytics-etl.env"

generate_password() {
  random_part=$(openssl rand -base64 24 | tr -d '\n=/+')
  printf 'Aa1@%s' "${random_part}"
}

if test -f "${API_ENV}"; then
  api_password=$(sed -n 's/^MYSQL_PASSWORD=//p' "${API_ENV}")
else
  api_password=$(generate_password)
fi
if test -f "${ETL_ENV}"; then
  etl_password=$(sed -n 's/^MYSQL_PASSWORD=//p' "${ETL_ENV}")
else
  etl_password=$(generate_password)
fi

case "${api_password}${etl_password}" in
  *[!A-Za-z0-9@]*) echo "Generated credential file has an invalid value" >&2; exit 1 ;;
esac

"${MYSQL_ADMIN[@]}" -e "CREATE DATABASE IF NOT EXISTS \`${MYSQL_DATABASE}\` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci"
"${MYSQL_ADMIN[@]}" "${MYSQL_DATABASE}" < "${SCHEMA_FILE}"
"${MYSQL_ADMIN[@]}" <<SQL
CREATE USER IF NOT EXISTS 'charging_api'@'%' IDENTIFIED BY '${api_password}';
ALTER USER 'charging_api'@'%' IDENTIFIED BY '${api_password}';
GRANT SELECT ON \`${MYSQL_DATABASE}\`.* TO 'charging_api'@'%';
CREATE USER IF NOT EXISTS 'charging_etl'@'127.0.0.1' IDENTIFIED BY '${etl_password}';
ALTER USER 'charging_etl'@'127.0.0.1' IDENTIFIED BY '${etl_password}';
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, CREATE TEMPORARY TABLES ON \`${MYSQL_DATABASE}\`.* TO 'charging_etl'@'127.0.0.1';
FLUSH PRIVILEGES;
SQL

printf '%s\n' \
  "MYSQL_HOST=${MYSQL_API_HOST}" \
  "MYSQL_PORT=${MYSQL_PORT}" \
  'MYSQL_USER=charging_api' \
  "MYSQL_PASSWORD=${api_password}" \
  "MYSQL_DATABASE=${MYSQL_DATABASE}" \
  'MYSQL_POOL_SIZE=5' \
  'ANALYTICS_HOST=0.0.0.0' \
  'ANALYTICS_PORT=8091' \
  'ANALYTICS_CORS_ORIGINS=http://localhost:5173' > "${API_ENV}"

printf '%s\n' \
  'MYSQL_HOST=127.0.0.1' \
  "MYSQL_PORT=${MYSQL_PORT}" \
  'MYSQL_USER=charging_etl' \
  "MYSQL_PASSWORD=${etl_password}" \
  "MYSQL_DATABASE=${MYSQL_DATABASE}" \
  "ADS_EXPORT_DIR=${SCRIPT_DIR}/../runtime/ads_export" > "${ETL_ENV}"

chmod 600 "${API_ENV}" "${ETL_ENV}"
echo "MySQL analytics users are ready"
echo "API environment: ${API_ENV}"
echo "ETL environment: ${ETL_ENV}"
