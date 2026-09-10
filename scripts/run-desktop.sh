#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
kind="${1:-user}"
case "$kind" in
  user) executable="apps/user-client/charging-user" ;;
  admin) executable="apps/admin-server/charging-admin" ;;
  *) echo "用法: bash scripts/run-desktop.sh user|admin [构建目录]"; exit 2 ;;
esac
build_dir="${2:-$project_dir/build}"
if [[ -z "${QT_IM_MODULE:-}" ]]; then
  plugins="$(qtpaths6 --query QT_INSTALL_PLUGINS 2>/dev/null || true)"
  if [[ -n "$plugins" ]]; then
    if pgrep -x fcitx5 >/dev/null && compgen -G "$plugins/platforminputcontexts/*fcitx5*" >/dev/null; then
      export QT_IM_MODULE=fcitx
    elif pgrep -x ibus-daemon >/dev/null && compgen -G "$plugins/platforminputcontexts/*ibus*" >/dev/null; then
      export QT_IM_MODULE=ibus
    fi
  fi
fi
if [[ ! -x "$build_dir/$executable" ]]; then
  echo "未找到可执行文件，请先构建项目或指定构建目录：$build_dir"; exit 1
fi
cd "$project_dir"
exec "$build_dir/$executable"
