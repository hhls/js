#!/usr/bin/env bash
# snell-server 交互式安装 / 更新 / 卸载脚本
# 用法: sudo bash install-snell.sh
# 默认下载示例: https://dl.nssurge.com/snell/snell-server-v5.0.1-linux-amd64.zip

set -euo pipefail

# ---------- 常量 ----------
BIN_PATH="/usr/local/bin/snell-server"
CONF_DIR="/etc/snell"
CONF_PATH="${CONF_DIR}/snell-server.conf"
SERVICE_NAME="snell"
SERVICE_PATH="/etc/systemd/system/${SERVICE_NAME}.service"
WORKDIR="/root/snell"
DEFAULT_URL="https://dl.nssurge.com/snell/snell-server-v5.0.1-linux-amd64.zip"

# ---------- 颜色 ----------
if [[ -t 1 ]]; then
  C_RESET='\033[0m'
  C_RED='\033[0;31m'
  C_GREEN='\033[0;32m'
  C_YELLOW='\033[0;33m'
  C_BLUE='\033[0;34m'
  C_CYAN='\033[0;36m'
  C_BOLD='\033[1m'
else
  C_RESET='' C_RED='' C_GREEN='' C_YELLOW='' C_BLUE='' C_CYAN='' C_BOLD=''
fi

info()  { echo -e "${C_CYAN}[信息]${C_RESET} $*"; }
ok()    { echo -e "${C_GREEN}[完成]${C_RESET} $*"; }
warn()  { echo -e "${C_YELLOW}[警告]${C_RESET} $*"; }
err()   { echo -e "${C_RED}[错误]${C_RESET} $*" >&2; }
title() { echo -e "\n${C_BOLD}${C_BLUE}======== $* ========${C_RESET}\n"; }

# ---------- 基础检查 ----------
require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    err "请使用 root 运行: sudo bash $0"
    exit 1
  fi
}

have_cmd() { command -v "$1" >/dev/null 2>&1; }

detect_pkg_manager() {
  if have_cmd apt-get; then
    echo "apt"
  elif have_cmd dnf; then
    echo "dnf"
  elif have_cmd yum; then
    echo "yum"
  elif have_cmd pacman; then
    echo "pacman"
  else
    echo "unknown"
  fi
}

install_deps() {
  local need=()
  have_cmd wget || have_cmd curl || need+=(wget)
  have_cmd unzip || need+=(unzip)

  if [[ ${#need[@]} -eq 0 ]]; then
    return 0
  fi

  info "安装依赖: ${need[*]}"
  local pm
  pm="$(detect_pkg_manager)"
  case "$pm" in
    apt)
      apt-get update -y
      apt-get install -y "${need[@]}"
      ;;
    dnf)
      dnf install -y "${need[@]}"
      ;;
    yum)
      yum install -y "${need[@]}"
      ;;
    pacman)
      pacman -Sy --noconfirm "${need[@]}"
      ;;
    *)
      err "无法自动安装依赖，请先手动安装: ${need[*]}"
      exit 1
      ;;
  esac
  ok "依赖已就绪"
}

download_file() {
  local url="$1"
  local dest="$2"
  if have_cmd wget; then
    wget -O "$dest" "$url"
  elif have_cmd curl; then
    curl -fL -o "$dest" "$url"
  else
    err "未找到 wget 或 curl"
    exit 1
  fi
}

# ---------- 状态 ----------
is_installed() { [[ -x "$BIN_PATH" ]]; }

is_service_exists() { [[ -f "$SERVICE_PATH" ]]; }

is_service_active() {
  have_cmd systemctl && systemctl is-active --quiet "${SERVICE_NAME}.service" 2>/dev/null
}

is_service_enabled() {
  have_cmd systemctl && systemctl is-enabled --quiet "${SERVICE_NAME}.service" 2>/dev/null
}

show_status() {
  title "当前状态"
  if is_installed; then
    ok "二进制: 已安装 -> ${BIN_PATH}"
    if "$BIN_PATH" --version >/dev/null 2>&1; then
      info "版本信息: $("$BIN_PATH" --version 2>&1 | head -n1 || true)"
    elif "$BIN_PATH" -v >/dev/null 2>&1; then
      info "版本信息: $("$BIN_PATH" -v 2>&1 | head -n1 || true)"
    else
      info "文件大小: $(ls -lh "$BIN_PATH" | awk '{print $5}')"
      info "修改时间: $(stat -c '%y' "$BIN_PATH" 2>/dev/null || stat -f '%Sm' "$BIN_PATH" 2>/dev/null || echo '未知')"
    fi
  else
    warn "二进制: 未安装"
  fi

  if [[ -f "$CONF_PATH" ]]; then
    ok "配置文件: 存在 -> ${CONF_PATH}"
  else
    warn "配置文件: 不存在"
  fi

  if is_service_exists; then
    ok "systemd 单元: 存在"
    if is_service_enabled; then
      info "开机自启: 已启用"
    else
      warn "开机自启: 未启用"
    fi
    if is_service_active; then
      ok "服务状态: 运行中"
    else
      warn "服务状态: 未运行"
    fi
  else
    warn "systemd 单元: 不存在"
  fi
  echo
}

# ---------- 输入 ----------
prompt() {
  # $1 提示 $2 默认值(可选) -> 写入 REPLY
  local tip="$1"
  local def="${2-}"
  if [[ -n "$def" ]]; then
    read -r -p "$(echo -e "${C_YELLOW}${tip}${C_RESET} [${def}]: ")" REPLY
    REPLY="${REPLY:-$def}"
  else
    read -r -p "$(echo -e "${C_YELLOW}${tip}${C_RESET}: ")" REPLY
  fi
}

confirm() {
  # 返回 0=是 1=否
  local tip="$1"
  local def="${2:-y}"
  local yn
  if [[ "$def" == "y" ]]; then
    read -r -p "$(echo -e "${C_YELLOW}${tip}${C_RESET} [Y/n]: ")" yn
    yn="${yn:-y}"
  else
    read -r -p "$(echo -e "${C_YELLOW}${tip}${C_RESET} [y/N]: ")" yn
    yn="${yn:-n}"
  fi
  [[ "$yn" =~ ^[Yy]$ ]]
}

ask_download_url() {
  title "下载地址"
  echo -e "示例: ${C_CYAN}${DEFAULT_URL}${C_RESET}"
  echo "可替换版本号，例如 snell-server-v4.1.1-linux-amd64.zip / arm64 等"
  echo
  prompt "请输入 snell-server 下载 URL" "$DEFAULT_URL"
  DOWNLOAD_URL="$REPLY"
  if [[ ! "$DOWNLOAD_URL" =~ ^https?:// ]]; then
    err "URL 格式不正确"
    return 1
  fi
  ZIP_NAME="$(basename "${DOWNLOAD_URL%%\?*}")"
  if [[ ! "$ZIP_NAME" =~ \.zip$ ]]; then
    warn "URL 看起来不是 .zip 文件: ${ZIP_NAME}"
    if ! confirm "仍然继续?" "n"; then
      return 1
    fi
  fi
  return 0
}

# ---------- 安装 / 更新核心 ----------
stop_service_if_running() {
  if is_service_active; then
    info "停止服务 ${SERVICE_NAME} ..."
    systemctl stop "${SERVICE_NAME}.service" || true
  fi
}

install_binary_from_url() {
  local url="$1"
  local tmpdir zipfile

  install_deps
  mkdir -p "$WORKDIR"
  tmpdir="$(mktemp -d /tmp/snell-install.XXXXXX)"
  # shellcheck disable=SC2064
  trap "rm -rf '$tmpdir'" RETURN

  zipfile="${tmpdir}/$(basename "${url%%\?*}")"
  info "下载: $url"
  download_file "$url" "$zipfile"
  ok "下载完成"

  info "解压到 /usr/local/bin ..."
  # 兼容 zip 内直接是 snell-server 或带路径
  unzip -o "$zipfile" -d "$tmpdir/extract"
  local binary
  binary="$(find "$tmpdir/extract" -type f -name 'snell-server' | head -n1)"
  if [[ -z "$binary" ]]; then
    # 有些包只有一个可执行文件
    binary="$(find "$tmpdir/extract" -type f -executable | head -n1)"
  fi
  if [[ -z "$binary" ]]; then
    # 再找任意非目录文件
    binary="$(find "$tmpdir/extract" -type f | head -n1)"
  fi
  if [[ -z "$binary" || ! -f "$binary" ]]; then
    err "压缩包中未找到 snell-server 可执行文件"
    ls -laR "$tmpdir/extract" || true
    exit 1
  fi

  stop_service_if_running
  install -m 755 "$binary" "$BIN_PATH"
  chmod +x "$BIN_PATH"
  ok "已安装二进制: $BIN_PATH"
}

write_systemd_unit() {
  mkdir -p "$(dirname "$SERVICE_PATH")"
  mkdir -p "$WORKDIR"
  cat >"$SERVICE_PATH" <<EOF
[Unit]
Description=snell
After=network.target

[Service]
User=root
WorkingDirectory=${WORKDIR}
ExecStart=${BIN_PATH} -c ${CONF_PATH}
Restart=on-failure
RestartSec=5
StartLimitBurst=15
StartLimitIntervalSec=300

[Install]
WantedBy=multi-user.target
EOF
  systemctl daemon-reload
  ok "已写入 systemd 单元: $SERVICE_PATH"
}

run_wizard() {
  mkdir -p "$CONF_DIR"
  if [[ -f "$CONF_PATH" ]]; then
    warn "配置文件已存在: $CONF_PATH"
    if confirm "是否备份后重新运行交互向导? (会覆盖配置)" "n"; then
      cp -a "$CONF_PATH" "${CONF_PATH}.bak.$(date +%Y%m%d%H%M%S)"
      info "已备份旧配置"
    else
      info "保留现有配置"
      return 0
    fi
  fi
  info "启动 snell-server 配置向导 (--wizard) ..."
  echo -e "${C_YELLOW}请按提示输入 listen / PSK 等参数${C_RESET}"
  "$BIN_PATH" --wizard -c "$CONF_PATH"
  ok "配置已写入: $CONF_PATH"
  echo
  info "当前配置内容:"
  cat "$CONF_PATH" || true
  echo
}

enable_and_start() {
  if ! have_cmd systemctl; then
    warn "系统无 systemctl，跳过服务注册。可手动运行:"
    echo "  $BIN_PATH -c $CONF_PATH"
    return 0
  fi
  write_systemd_unit
  systemctl enable "${SERVICE_NAME}.service"
  systemctl restart "${SERVICE_NAME}.service"
  sleep 1
  systemctl --no-pager --full status "${SERVICE_NAME}.service" || true
  if is_service_active; then
    ok "服务已启动并设置开机自启"
  else
    err "服务未能正常启动，请检查: journalctl -u ${SERVICE_NAME} -n 50 --no-pager"
    return 1
  fi
}

do_install() {
  title "安装 snell-server"
  if is_installed; then
    warn "检测到已安装 snell-server"
    if ! confirm "继续将覆盖二进制文件，是否继续?" "y"; then
      info "已取消"
      return 0
    fi
  fi
  ask_download_url || return 1
  install_binary_from_url "$DOWNLOAD_URL"
  run_wizard
  if confirm "是否注册并启动 systemd 服务 (开机自启)?" "y"; then
    enable_and_start
  else
    info "跳过服务注册。手动启动: $BIN_PATH -c $CONF_PATH"
  fi
  show_status
  ok "安装流程结束"
}

do_update() {
  title "更新 / 覆盖 snell-server"
  if ! is_installed; then
    warn "尚未安装，将走完整安装流程"
    do_install
    return
  fi
  ask_download_url || return 1
  info "将下载并覆盖现有二进制，默认保留配置文件"
  install_binary_from_url "$DOWNLOAD_URL"

  if [[ -f "$CONF_PATH" ]]; then
    if confirm "是否重新运行配置向导? (一般更新版本不需要)" "n"; then
      run_wizard
    else
      ok "保留现有配置: $CONF_PATH"
    fi
  else
    warn "未找到配置文件，将运行向导"
    run_wizard
  fi

  if is_service_exists || confirm "是否写入/更新 systemd 并启动服务?" "y"; then
    enable_and_start
  fi
  show_status
  ok "更新完成"
}

do_uninstall() {
  title "卸载 snell-server"
  if ! confirm "确认卸载 snell-server (可选择是否删除配置)?" "n"; then
    info "已取消"
    return 0
  fi

  if is_service_exists && have_cmd systemctl; then
    info "停止并禁用服务 ..."
    systemctl stop "${SERVICE_NAME}.service" 2>/dev/null || true
    systemctl disable "${SERVICE_NAME}.service" 2>/dev/null || true
    rm -f "$SERVICE_PATH"
    systemctl daemon-reload
    ok "已移除 systemd 单元"
  fi

  if [[ -x "$BIN_PATH" ]]; then
    rm -f "$BIN_PATH"
    ok "已删除二进制: $BIN_PATH"
  fi

  if [[ -d "$CONF_DIR" ]]; then
    if confirm "是否同时删除配置目录 ${CONF_DIR}?" "n"; then
      rm -rf "$CONF_DIR"
      ok "已删除配置目录"
    else
      info "保留配置目录: $CONF_DIR"
    fi
  fi

  if [[ -d "$WORKDIR" ]]; then
    if confirm "是否删除工作目录 ${WORKDIR}?" "n"; then
      rm -rf "$WORKDIR"
      ok "已删除工作目录"
    fi
  fi

  show_status
  ok "卸载完成"
}

do_edit_config() {
  title "编辑配置"
  if [[ ! -f "$CONF_PATH" ]]; then
    warn "配置不存在"
    if confirm "是否运行配置向导生成?" "y"; then
      if ! is_installed; then
        err "请先安装 snell-server"
        return 1
      fi
      run_wizard
    fi
    return 0
  fi
  local editor="${EDITOR:-}"
  if [[ -z "$editor" ]]; then
    if have_cmd vim; then editor=vim
    elif have_cmd nano; then editor=nano
    elif have_cmd vi; then editor=vi
    else
      err "未找到编辑器，请设置 EDITOR 环境变量"
      return 1
    fi
  fi
  "$editor" "$CONF_PATH"
  if is_service_exists && confirm "配置已修改，是否重启服务?" "y"; then
    systemctl restart "${SERVICE_NAME}.service"
    systemctl --no-pager --full status "${SERVICE_NAME}.service" || true
  fi
}

do_service_ops() {
  title "服务管理"
  if ! have_cmd systemctl; then
    err "当前系统不支持 systemctl"
    return 1
  fi
  if ! is_service_exists; then
    warn "服务单元不存在"
    if is_installed && [[ -f "$CONF_PATH" ]] && confirm "现在创建并启动服务?" "y"; then
      enable_and_start
    fi
    return 0
  fi
  echo "  1) 启动   start"
  echo "  2) 停止   stop"
  echo "  3) 重启   restart"
  echo "  4) 状态   status"
  echo "  5) 日志   journalctl"
  echo "  0) 返回"
  prompt "请选择" "4"
  case "$REPLY" in
    1) systemctl start "${SERVICE_NAME}.service"; systemctl status --no-pager "${SERVICE_NAME}.service" || true ;;
    2) systemctl stop "${SERVICE_NAME}.service"; ok "已停止" ;;
    3) systemctl restart "${SERVICE_NAME}.service"; systemctl status --no-pager "${SERVICE_NAME}.service" || true ;;
    4) systemctl --no-pager --full status "${SERVICE_NAME}.service" || true ;;
    5) journalctl -u "${SERVICE_NAME}" -n 80 --no-pager || true ;;
    0) return 0 ;;
    *) warn "无效选项" ;;
  esac
}

do_show_config() {
  title "查看配置"
  if [[ -f "$CONF_PATH" ]]; then
    cat "$CONF_PATH"
  else
    warn "配置文件不存在: $CONF_PATH"
  fi
  echo
}

# ---------- 主菜单 ----------
print_banner() {
  echo -e "${C_BOLD}${C_CYAN}"
  cat <<'EOF'
   _____           _ _   _____           _        _ _
  / ____|         | | | |_   _|         | |      | | |
 | (___  _ __   __| | |   | |  _ __  ___| |_ __ _| | |
  \___ \| '_ \ / _` | |   | | | '_ \/ __| __/ _` | | |
  ____) | | | | (_| | |  _| |_| | | \__ \ || (_| | | |
 |_____/|_| |_|\__,_|_| |_____|_| |_|___/\__\__,_|_|_|
EOF
  echo -e "${C_RESET}"
  echo -e "  ${C_BOLD}snell-server 交互式安装脚本${C_RESET}"
  echo -e "  支持: 安装 / 覆盖更新 / 卸载 / 服务与配置管理"
  echo
}

main_menu() {
  while true; do
    print_banner
    show_status
    echo -e "${C_BOLD}请选择操作:${C_RESET}"
    echo "  1) 全新安装 (下载 + 配置向导 + systemd)"
    echo "  2) 更新 / 覆盖版本 (输入新 URL，默认保留配置)"
    echo "  3) 服务管理 (启停/重启/状态/日志)"
    echo "  4) 查看配置"
    echo "  5) 编辑配置"
    echo "  6) 重新运行配置向导"
    echo "  7) 卸载"
    echo "  0) 退出"
    echo
    prompt "输入序号" "1"
    case "$REPLY" in
      1) do_install ;;
      2) do_update ;;
      3) do_service_ops ;;
      4) do_show_config ;;
      5) do_edit_config ;;
      6)
        if is_installed; then run_wizard
          if is_service_exists && confirm "是否重启服务?" "y"; then
            systemctl restart "${SERVICE_NAME}.service"
          fi
        else
          err "请先安装"
        fi
        ;;
      7) do_uninstall ;;
      0) info "再见"; exit 0 ;;
      *) warn "无效选项，请重试" ;;
    esac
    echo
    read -r -p "$(echo -e "${C_CYAN}按回车返回主菜单...${C_RESET}")" _
  done
}

# ---------- 非交互参数 (可选) ----------
usage() {
  cat <<EOF
用法:
  sudo bash $0                 # 交互菜单
  sudo bash $0 install [URL]   # 直接安装
  sudo bash $0 update  [URL]   # 直接更新
  sudo bash $0 uninstall       # 卸载
  sudo bash $0 status          # 查看状态
  sudo bash $0 restart         # 重启服务

默认 URL:
  ${DEFAULT_URL}
EOF
}

main() {
  require_root

  local action="${1-}"
  case "$action" in
    ""|menu)
      main_menu
      ;;
    install)
      if [[ -n "${2-}" ]]; then
        DOWNLOAD_URL="$2"
        install_binary_from_url "$DOWNLOAD_URL"
        run_wizard
        enable_and_start
        show_status
      else
        do_install
      fi
      ;;
    update)
      if [[ -n "${2-}" ]]; then
        DOWNLOAD_URL="$2"
        install_binary_from_url "$DOWNLOAD_URL"
        enable_and_start
        show_status
      else
        do_update
      fi
      ;;
    uninstall)
      do_uninstall
      ;;
    status)
      show_status
      if is_service_exists; then
        systemctl --no-pager --full status "${SERVICE_NAME}.service" || true
      fi
      ;;
    restart)
      systemctl restart "${SERVICE_NAME}.service"
      systemctl --no-pager status "${SERVICE_NAME}.service" || true
      ;;
    -h|--help|help)
      usage
      ;;
    *)
      err "未知参数: $action"
      usage
      exit 1
      ;;
  esac
}

main "$@"
