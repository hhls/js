#!/usr/bin/env bash
#
# hy2.sh - 基于官方 get.hy2.sh 的 Hysteria2 交互式安装/配置
#
# 原则：防 QoS 第一（少打满、少冲带宽、行为克制）> 连接稳定 > 极限吞吐
# 能力：端口跳跃、HTTP/3 伪装、可选混淆、偏低带宽 + 关 Brutal 丢包补偿、
#       UDP 收发缓冲调优（减内核丢包，属稳定向；不追求把链路打满）
# 参考：
#   https://hysteria.network/zh/docs/advanced/Full-Server-Config/
#   https://v2.hysteria.network/zh/docs/advanced/Performance/
#
set -euo pipefail

SCRIPT_NAME="$(basename "$0")"
CONFIG_DIR="/etc/hysteria"
CONFIG_FILE="${CONFIG_DIR}/config.yaml"
CERT_FILE="${CONFIG_DIR}/server.crt"
KEY_FILE="${CONFIG_DIR}/server.key"
# 记录上次导出目录，便于菜单再次查看
EXPORT_META="${CONFIG_DIR}/export-dir"
SERVICE_NAME="hysteria-server.service"
HY2_BIN="/usr/local/bin/hysteria"
OFFICIAL_INSTALL_URL="https://get.hy2.sh/"

# 稳定性：UDP/socket 缓冲区（官方 Performance 推荐 16MB；只升不降）
SYSCTL_DROPIN="/etc/sysctl.d/99-hysteria-udp-buffer.conf"
SYSCTL_BACKUP="${CONFIG_DIR}/sysctl-udp-buffer.prev"
UDP_BUF_TARGET=16777216

# 默认值（服务端 up≈客户端下载，down≈客户端上传；故意偏低防 QoS）
DEFAULT_BW_UP="100 mbps"
DEFAULT_BW_DOWN="30 mbps"
DEFAULT_HOP_RANGE="20000-50000"
DEFAULT_LISTEN_PORT="443"
DEFAULT_MASQ_URL="https://www.bing.com/"
DEFAULT_HOP_INTERVAL="30s"
# 连接信息默认保存到「执行时的当前目录」

# --- 交互收集的配置（全局，减少超长参数列表）---
CFG_LISTEN=""
CFG_IS_RANGE=0
CFG_HOP_INTERVAL="$DEFAULT_HOP_INTERVAL"
CFG_CERT_MODE="self"          # acme | self
CFG_DOMAIN=""
CFG_EMAIL=""
CFG_SNI=""
CFG_INSECURE=1
CFG_AUTH_PASS=""
CFG_MASQ_TYPE="proxy"         # proxy | string | none
CFG_MASQ_URL="$DEFAULT_MASQ_URL"
CFG_MASQ_STRING="ok"
CFG_MASQ_TCP=0                # listenHTTP/HTTPS 做全套伪装
CFG_OBFS=0                    # 0 关 | 1 salamander
CFG_OBFS_PASS=""
CFG_BW_UP="$DEFAULT_BW_UP"
CFG_BW_DOWN="$DEFAULT_BW_DOWN"
CFG_DISABLE_LOSS_COMP=1       # 关 Brutal 丢包补偿：防 QoS / 少冲带宽
CFG_SNIFF=1                   # 协议嗅探，TUN/ACL 更友好
CFG_SPEEDTEST=0
CFG_SERVER_ADDR=""
CFG_EXPORT_DIR=""

# colors
if [[ -t 1 ]] && command -v tput >/dev/null 2>&1; then
  C_RED=$(tput setaf 1)
  C_GREEN=$(tput setaf 2)
  C_YELLOW=$(tput setaf 3)
  C_BLUE=$(tput setaf 4)
  C_BOLD=$(tput bold)
  C_RESET=$(tput sgr0)
else
  C_RED= C_GREEN= C_YELLOW= C_BLUE= C_BOLD= C_RESET=
fi

info()  { echo -e "${C_GREEN}[+]${C_RESET} $*"; }
warn()  { echo -e "${C_YELLOW}[!]${C_RESET} $*"; }
err()   { echo -e "${C_RED}[x]${C_RESET} $*" >&2; }
title() { echo -e "\n${C_BOLD}${C_BLUE}==> $*${C_RESET}"; }

need_root() {
  if [[ "$(id -u)" -ne 0 ]]; then
    err "请使用 root 运行：sudo bash $SCRIPT_NAME"
    exit 1
  fi
}

has_cmd() { command -v "$1" >/dev/null 2>&1; }

prompt() {
  local msg="$1"
  local def="${2-}"
  local ans
  if [[ -n "$def" ]]; then
    read -r -p "$(echo -e "${C_BOLD}${msg}${C_RESET} [${def}]: ")" ans || true
    ans="${ans:-$def}"
  else
    read -r -p "$(echo -e "${C_BOLD}${msg}${C_RESET}: ")" ans || true
  fi
  printf '%s' "$ans"
}

prompt_yn() {
  local msg="$1"
  local def="${2:-y}"
  local ans
  while true; do
    ans="$(prompt "$msg (y/n)" "$def")"
    case "${ans,,}" in
      y|yes) return 0 ;;
      n|no)  return 1 ;;
      *) warn "请输入 y 或 n" ;;
    esac
  done
}

rand_password() {
  if has_cmd openssl; then
    openssl rand -base64 18 | tr -d '=+/' | cut -c1-20
  else
    tr -dc 'A-Za-z0-9' </dev/urandom | head -c 20
  fi
}

# 简单 YAML 双引号
yaml_quote() {
  local s="${1//\\/\\\\}"
  s="${s//\"/\\\"}"
  printf '"%s"' "$s"
}

# URI 组件百分号编码
urlencode() {
  local s="$1"
  if has_cmd python3; then
    python3 -c 'import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1], safe=""))' "$s"
    return
  fi
  # 纯 bash 回退
  local i c out=""
  local LC_ALL=C
  for ((i = 0; i < ${#s}; i++)); do
    c="${s:i:1}"
    case "$c" in
      [a-zA-Z0-9.~_-]) out+="$c" ;;
      *) printf -v c '%%%02X' "'$c"; out+="$c" ;;
    esac
  done
  printf '%s' "$out"
}

detect_public_ip() {
  local ip=""
  ip="$(curl -fsS --max-time 5 https://api.ipify.org 2>/dev/null || true)"
  [[ -z "$ip" ]] && ip="$(curl -fsS --max-time 5 https://ifconfig.me 2>/dev/null || true)"
  [[ -z "$ip" ]] && ip="$(hostname -I 2>/dev/null | awk '{print $1}')"
  printf '%s' "$ip"
}

cert_sha256_pin() {
  if [[ -f "$CERT_FILE" ]] && has_cmd openssl; then
    openssl x509 -in "$CERT_FILE" -noout -fingerprint -sha256 2>/dev/null \
      | cut -d= -f2 | tr -d ':' | tr 'A-F' 'a-f'
  fi
}

pkg_install() {
  local pkgs=("$@")
  ((${#pkgs[@]})) || return 0
  info "安装依赖: ${pkgs[*]}"
  if has_cmd apt-get; then
    apt-get update -y
    DEBIAN_FRONTEND=noninteractive apt-get install -y "${pkgs[@]}"
  elif has_cmd dnf; then
    dnf install -y "${pkgs[@]}"
  elif has_cmd yum; then
    yum install -y "${pkgs[@]}"
  elif has_cmd apk; then
    apk add --no-cache "${pkgs[@]}"
  else
    err "缺少依赖: ${pkgs[*]}，请先手动安装"
    exit 1
  fi
}

ensure_deps() {
  local missing=()
  for c in curl openssl; do
    has_cmd "$c" || missing+=("$c")
  done
  if ((${#missing[@]})); then
    pkg_install "${missing[@]}"
  fi
}

# 端口范围 listen（官方内置端口跳跃）会自动调用 iptables/nftables 做 UDP DNAT
# 参考: HYSTERIA_FIREWALL_BACKEND / Port-Hopping built-in port range (Linux)
# 仅有命令不够：容器/OpenVZ 等常缺 CAP_NET_ADMIN，nft 会报 Operation not permitted

# 探测 netfilter 是否真正可写（与 Hysteria 启动时 add table 同类权限）
probe_netfilter_write() {
  local tag="hy2_probe_$$"
  if has_cmd nft; then
    if nft add table ip "$tag" 2>/dev/null; then
      nft delete table ip "$tag" 2>/dev/null || true
      return 0
    fi
  fi
  if has_cmd iptables; then
    if iptables -t nat -N "$tag" 2>/dev/null; then
      iptables -t nat -X "$tag" 2>/dev/null || true
      return 0
    fi
  fi
  return 1
}

maybe_container_hint() {
  if [[ -f /.dockerenv ]] || [[ -f /run/.containerenv ]] \
    || grep -qaE 'docker|lxc|kubepods|containerd|libpod|podman' /proc/1/cgroup 2>/dev/null \
    || [[ -d /proc/vz && ! -d /proc/bc ]]; then
    echo "  当前环境像容器/OpenVZ：端口跳跃要在「容器网络命名空间」里写 nft/iptables"
    echo "  · Docker/Podman rootful：--cap-add=NET_ADMIN（建议再加 --network=host）"
    echo "  · Podman rootless：通常做不到（用户命名空间限制），请用单端口或 rootful+host 网络"
    echo "  · 或直接在宿主机 / 完整 VPS 上跑，不进容器"
  fi
}

ensure_firewall_for_porthop() {
  if ! has_cmd iptables && ! has_cmd nft; then
    warn "端口跳跃需要 iptables 或 nftables（当前均未找到）"
    local pkgs=()
    if has_cmd apt-get; then
      pkgs+=(iptables nftables)
    elif has_cmd dnf || has_cmd yum; then
      pkgs+=(iptables nftables)
    elif has_cmd apk; then
      pkgs+=(iptables nftables)
    else
      err "请先安装 iptables 或 nftables 后再启用端口跳跃"
      exit 1
    fi
    pkg_install "${pkgs[@]}"
  fi
  if ! has_cmd iptables && ! has_cmd nft; then
    err "安装后仍找不到 iptables/nft，无法启用端口跳跃"
    exit 1
  fi
  info "防火墙工具: $(has_cmd iptables && echo -n 'iptables '; has_cmd nft && echo -n 'nftables')"
  if probe_netfilter_write; then
    info "netfilter 可写，端口跳跃可用"
    return 0
  fi
  err "无法写入 netfilter（常见错误: Operation not permitted）"
  err "端口跳跃需要 CAP_NET_ADMIN；仅 root 或装了 nft/iptables 不够"
  maybe_container_hint
  echo "  处理办法："
  echo "    1) 重新配置时选「不启用端口跳跃」（单端口即可正常跑）"
  echo "    2) Docker/Podman rootful 示例："
  echo "       --cap-add=NET_ADMIN --network=host"
  echo "       （rootless Podman 一般仍会失败，见上方说明）"
  echo "    3) 换到完整 VPS / 宿主机再开端口跳跃"
  exit 1
}

# 启动失败时根据 journal 给可操作提示（避免只甩 journalctl）
diagnose_service_failure() {
  local log
  log="$(journalctl -u "$SERVICE_NAME" -n 30 --no-pager 2>/dev/null || true)"
  if echo "$log" | grep -qiE 'Operation not permitted|add table|nft |iptables'; then
    warn "日志像是「端口跳跃改防火墙失败」（nft/iptables 无权限）"
    echo "  配置里 listen 若是 起始-结束 范围，服务端会调 nft/iptables 做 UDP 重定向"
    echo "  当前环境没有 CAP_NET_ADMIN 时会启动失败"
    maybe_container_hint
    echo "  建议：菜单选 2 重新配置 → 端口跳跃选 n（单端口）"
    return 0
  fi
  if echo "$log" | grep -qiE 'permission denied|bind: address already in use|address already in use'; then
    warn "日志像是端口被占用或权限不足，请对照 journal 中 FATAL 行处理"
  fi
}

fix_owner() {
  local f
  for f in "$@"; do
    [[ -e "$f" ]] || continue
    if id hysteria >/dev/null 2>&1; then
      chown hysteria:hysteria "$f" 2>/dev/null || true
    fi
  done
}

# --- 稳定性调优：系统 UDP 收发缓冲（官方 Performance；不改 quic 窗口 / 实时优先级）---

is_linux() { [[ "$(uname -s 2>/dev/null)" == "Linux" ]]; }

# 读 sysctl 数值；失败返回空
read_sysctl_int() {
  local key="$1" val=""
  if has_cmd sysctl; then
    val="$(sysctl -n "$key" 2>/dev/null || true)"
  fi
  if [[ -z "$val" && -r "/proc/sys/${key//./\/}" ]]; then
    val="$(tr -d ' \t\r\n' <"/proc/sys/${key//./\/}" 2>/dev/null || true)"
  fi
  [[ "$val" =~ ^[0-9]+$ ]] && printf '%s' "$val" || printf ''
}

# 仅当当前值 < 目标时写入；返回 0=已提高或已达标，1=失败
raise_sysctl_if_lower() {
  local key="$1" target="$2" cur
  cur="$(read_sysctl_int "$key")"
  if [[ -z "$cur" ]]; then
    warn "无法读取 $key，跳过"
    return 1
  fi
  if ((cur >= target)); then
    info "$key = $cur（已 ≥ ${target}，不降低）"
    return 0
  fi
  if sysctl -w "${key}=${target}" >/dev/null 2>&1; then
    info "$key: ${cur} → ${target}"
    return 0
  fi
  err "无法设置 $key=${target}（权限或内核限制？）"
  return 1
}

udp_buffer_status() {
  local rmem wmem
  rmem="$(read_sysctl_int net.core.rmem_max)"
  wmem="$(read_sysctl_int net.core.wmem_max)"
  title "稳定性调优状态（UDP 缓冲区）"
  if ! is_linux; then
    warn "当前非 Linux，官方 sysctl 调优仅适用于 Linux 服务端"
    return 0
  fi
  echo "  目标值     : ${UDP_BUF_TARGET} (16 MB，官方 Performance 推荐)"
  echo "  rmem_max   : ${rmem:-未知}"
  echo "  wmem_max   : ${wmem:-未知}"
  if [[ -f "$SYSCTL_DROPIN" ]]; then
    info "持久化文件: $SYSCTL_DROPIN（已安装，重启后仍生效）"
  else
    warn "持久化文件: 未安装（仅可能为运行时临时值）"
  fi
  if [[ -f "$SYSCTL_BACKUP" ]]; then
    info "备份（撤销用）: $SYSCTL_BACKUP"
    cat "$SYSCTL_BACKUP" 2>/dev/null | sed 's/^/    /' || true
  fi
  if [[ -n "$rmem" && -n "$wmem" ]] && ((rmem >= UDP_BUF_TARGET && wmem >= UDP_BUF_TARGET)); then
    info "运行时缓冲已达标（≥ 16MB）"
  elif [[ -n "$rmem" || -n "$wmem" ]]; then
    warn "运行时缓冲未达 16MB，突发 UDP 时内核更易丢包 → 可执行「应用稳定性调优」"
  fi
}

# 应用：写 sysctl.d + 运行时只升不降；首次备份旧值便于撤销
apply_udp_buffer_tune() {
  title "应用稳定性调优（UDP 缓冲区）"
  if ! is_linux; then
    err "仅支持 Linux（官方 net.core.rmem_max / wmem_max）"
    return 1
  fi
  if ! has_cmd sysctl; then
    err "未找到 sysctl，请先安装 procps（或发行版等价包）"
    return 1
  fi
  if [[ ! -d /etc/sysctl.d ]]; then
    err "不存在 /etc/sysctl.d，无法持久化（可能是精简容器环境）"
    return 1
  fi

  local cur_r cur_w
  cur_r="$(read_sysctl_int net.core.rmem_max)"
  cur_w="$(read_sysctl_int net.core.wmem_max)"
  if [[ -z "$cur_r" || -z "$cur_w" ]]; then
    err "无法读取当前 net.core.rmem_max / wmem_max"
    return 1
  fi

  # 首次应用时备份，便于 revert 恢复
  if [[ ! -f "$SYSCTL_BACKUP" ]]; then
    mkdir -p "$CONFIG_DIR"
    cat >"$SYSCTL_BACKUP" <<EOF
# previous values saved by $SCRIPT_NAME $(date -Iseconds 2>/dev/null || date)
net.core.rmem_max=${cur_r}
net.core.wmem_max=${cur_w}
EOF
    chmod 644 "$SYSCTL_BACKUP" 2>/dev/null || true
    info "已备份当前值到 $SYSCTL_BACKUP"
  else
    info "已有备份 $SYSCTL_BACKUP（保留首次备份，不覆盖）"
  fi

  cat >"$SYSCTL_DROPIN" <<EOF
# Managed by $SCRIPT_NAME — Hysteria2 stability (UDP socket buffers)
# Ref: https://v2.hysteria.network/zh/docs/advanced/Performance/
# Only raises defaults to 16MB; does not touch quic windows or CPU priority.
net.core.rmem_max = ${UDP_BUF_TARGET}
net.core.wmem_max = ${UDP_BUF_TARGET}
EOF
  chmod 644 "$SYSCTL_DROPIN"
  info "已写入 $SYSCTL_DROPIN"

  local ok=0
  raise_sysctl_if_lower net.core.rmem_max "$UDP_BUF_TARGET" && ok=$((ok + 1)) || true
  raise_sysctl_if_lower net.core.wmem_max "$UDP_BUF_TARGET" && ok=$((ok + 1)) || true
  if ((ok < 2)); then
    warn "运行时应用未完全成功；持久化文件已写，重启后可能仍会加载"
    warn "容器/无特权环境可能无法改 sysctl（需特权或宿主机调优）"
    return 1
  fi
  info "稳定性调优已应用（rmem_max / wmem_max ≥ ${UDP_BUF_TARGET}）"
  info "说明: 未改 quic 流控窗口、未提高进程实时优先级；防 QoS 仍靠带宽与丢包补偿策略"
  return 0
}

# 撤销：删 drop-in，尽量恢复备份值
revert_udp_buffer_tune() {
  title "撤销稳定性调优（UDP 缓冲区）"
  if ! is_linux; then
    err "仅支持 Linux"
    return 1
  fi
  local had=0
  if [[ -f "$SYSCTL_DROPIN" ]]; then
    rm -f "$SYSCTL_DROPIN"
    info "已删除 $SYSCTL_DROPIN"
    had=1
  else
    warn "未找到 $SYSCTL_DROPIN"
  fi

  local rest_r="" rest_w=""
  if [[ -f "$SYSCTL_BACKUP" ]]; then
    rest_r="$(grep -E '^net\.core\.rmem_max=' "$SYSCTL_BACKUP" 2>/dev/null | tail -1 | cut -d= -f2 | tr -d ' \t\r\n' || true)"
    rest_w="$(grep -E '^net\.core\.wmem_max=' "$SYSCTL_BACKUP" 2>/dev/null | tail -1 | cut -d= -f2 | tr -d ' \t\r\n' || true)"
  fi

  if [[ "$rest_r" =~ ^[0-9]+$ ]] && [[ "$rest_w" =~ ^[0-9]+$ ]]; then
    if has_cmd sysctl; then
      if sysctl -w "net.core.rmem_max=${rest_r}" >/dev/null 2>&1 \
        && sysctl -w "net.core.wmem_max=${rest_w}" >/dev/null 2>&1; then
        info "运行时已恢复: rmem_max=${rest_r} wmem_max=${rest_w}"
      else
        warn "无法写回运行时 sysctl，重启后将不再加载本脚本的 drop-in"
      fi
    fi
    rm -f "$SYSCTL_BACKUP"
    info "已删除备份 $SYSCTL_BACKUP"
    had=1
  else
    warn "无有效备份：仅移除了 drop-in；运行时数值保持不变，直到重启或其它配置覆盖"
  fi

  if ((had == 0)); then
    info "无需撤销（未应用过）"
  else
    info "撤销完成"
  fi
  return 0
}

# 安装/重配末尾：稳优先默认直接应用缓冲调优（仅在用户明确拒绝时跳过）
maybe_apply_stability_tune() {
  if ! is_linux; then
    return 0
  fi
  echo
  title "系统缓冲调优（减内核丢包 · 推荐默认应用）"
  echo "  将 net.core.rmem_max / wmem_max 提到 16MB（官方 Performance）"
  echo "  作用：减少高负载下 UDP 在内核被丢掉 → 少抖动（不是靠冲带宽躲 QoS）"
  echo "  防 QoS 仍靠：偏低带宽 + 关 Brutal 丢包补偿 + 伪装/跳端口等"
  echo "  不做：不改 quic 窗口、不设实时优先级"
  echo "  可逆：菜单 11 / untune；卸载时可一并撤销"
  local rmem wmem
  rmem="$(read_sysctl_int net.core.rmem_max)"
  wmem="$(read_sysctl_int net.core.wmem_max)"
  if [[ -n "$rmem" && -n "$wmem" ]] && ((rmem >= UDP_BUF_TARGET && wmem >= UDP_BUF_TARGET)); then
    info "缓冲已达标（rmem=${rmem} wmem=${wmem}）"
    # 补齐持久化 drop-in（重启后仍保持稳）
    if [[ ! -f "$SYSCTL_DROPIN" ]]; then
      info "运行时已达标但缺持久化文件，补写 $SYSCTL_DROPIN"
      apply_udp_buffer_tune || true
    fi
    return 0
  fi
  # 默认应用；只有明确选 n 才跳过
  if prompt_yn "应用 UDP 缓冲区调优？（减内核丢包，默认应用）" "y"; then
    apply_udp_buffer_tune || warn "调优未完全成功，可稍后: sudo bash $SCRIPT_NAME tune"
  else
    warn "已跳过：高负载时更易内核丢 UDP（与运营商 QoS 是两回事）"
    info "需要时: sudo bash $SCRIPT_NAME tune"
  fi
}

stability_tune_menu() {
  need_root
  while true; do
    echo
    echo -e "${C_BOLD}------ 稳定性调优（UDP 缓冲）------${C_RESET}"
    echo "  1) 查看状态"
    echo "  2) 应用调优（推荐，只升不降）"
    echo "  3) 撤销调优"
    echo "  0) 返回"
    local c
    c="$(prompt "请选择" "1")"
    case "$c" in
      1) udp_buffer_status ;;
      2) apply_udp_buffer_tune ;;
      3)
        if prompt_yn "确认撤销 UDP 缓冲区调优？" "n"; then
          revert_udp_buffer_tune
        else
          info "已取消"
        fi
        ;;
      0) return 0 ;;
      *) warn "无效选项" ;;
    esac
  done
}

install_binary() {
  title "官方脚本安装 / 升级 Hysteria2"
  if [[ -x "$HY2_BIN" ]]; then
    local ver
    ver="$($HY2_BIN version 2>/dev/null | head -1 || echo "$HY2_BIN")"
    info "本机已安装: $ver"
    echo "  y = 调用官方脚本，升级到最新版"
    echo "  n = 不升级，保留当前版本，直接进入配置（默认）"
    if ! prompt_yn "是否升级二进制？" "n"; then
      info "已跳过升级，继续使用当前版本"
      return 0
    fi
  fi
  bash <(curl -fsSL "$OFFICIAL_INSTALL_URL")
  if [[ ! -x "$HY2_BIN" ]]; then
    err "安装失败：找不到 $HY2_BIN"
    exit 1
  fi
  info "安装完成: $($HY2_BIN version 2>/dev/null | head -1 || true)"
}

gen_self_signed_cert() {
  local cn="$1"
  mkdir -p "$CONFIG_DIR"
  openssl req -x509 -nodes -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 \
    -keyout "$KEY_FILE" -out "$CERT_FILE" \
    -subj "/CN=${cn}" -days 3650 >/dev/null 2>&1
  fix_owner "$CERT_FILE" "$KEY_FILE"
  chmod 600 "$KEY_FILE"
  chmod 644 "$CERT_FILE"
  info "已生成自签证书 CN=${cn}"
}

write_server_config() {
  mkdir -p "$CONFIG_DIR"
  {
    echo "# generated by $SCRIPT_NAME on $(date -Iseconds 2>/dev/null || date)"
    echo "# ref: https://hysteria.network/zh/docs/advanced/Full-Server-Config/"
    echo "listen: :${CFG_LISTEN}"
    echo

    if [[ "$CFG_CERT_MODE" == "acme" ]]; then
      cat <<EOF
acme:
  domains:
    - $(yaml_quote "$CFG_DOMAIN")
  email: $(yaml_quote "$CFG_EMAIL")
EOF
    else
      cat <<EOF
tls:
  cert: ${CERT_FILE}
  key: ${KEY_FILE}
EOF
    fi

    cat <<EOF

auth:
  type: password
  password: $(yaml_quote "$CFG_AUTH_PASS")

bandwidth:
  up: $(yaml_quote "$CFG_BW_UP")
  down: $(yaml_quote "$CFG_BW_DOWN")
EOF
    if [[ "$CFG_DISABLE_LOSS_COMP" == "1" ]]; then
      echo "  disableLossCompensation: true"
    fi

    # 协议嗅探：TUN 入站时把 IP 还原成域名，ACL/分流更准
    if [[ "$CFG_SNIFF" == "1" ]]; then
      cat <<EOF

sniff:
  enable: true
  timeout: 2s
  rewriteDomain: false
  tcpPorts: 80,443,8000-9000
  udpPorts: all
EOF
    fi

    if [[ "$CFG_SPEEDTEST" == "1" ]]; then
      echo
      echo "speedTest: true"
    fi

    if [[ "$CFG_OBFS" == "1" ]]; then
      cat <<EOF

obfs:
  type: salamander
  salamander:
    password: $(yaml_quote "$CFG_OBFS_PASS")
EOF
    fi

    if [[ "$CFG_MASQ_TYPE" == "proxy" ]]; then
      cat <<EOF

masquerade:
  type: proxy
  proxy:
    url: $(yaml_quote "$CFG_MASQ_URL")
    rewriteHost: true
EOF
      if [[ "$CFG_MASQ_TCP" == "1" ]]; then
        cat <<EOF
  listenHTTP: :80
  listenHTTPS: :443
  forceHTTPS: true
EOF
      fi
    elif [[ "$CFG_MASQ_TYPE" == "string" ]]; then
      cat <<EOF

masquerade:
  type: string
  string:
    content: $(yaml_quote "$CFG_MASQ_STRING")
    headers:
      content-type: text/plain
EOF
      if [[ "$CFG_MASQ_TCP" == "1" ]]; then
        cat <<EOF
  listenHTTP: :80
  listenHTTPS: :443
  forceHTTPS: true
EOF
      fi
    fi
  } >"$CONFIG_FILE"

  fix_owner "$CONFIG_FILE"
  chmod 600 "$CONFIG_FILE"
  info "已写入服务端配置: $CONFIG_FILE"
}

# 生成分享 URI（hysteria2://）
build_share_uri() {
  local auth_enc hostport query pin
  auth_enc="$(urlencode "$CFG_AUTH_PASS")"
  hostport="${CFG_SERVER_ADDR}:${CFG_LISTEN}"
  query="sni=$(urlencode "$CFG_SNI")"
  if [[ "$CFG_INSECURE" == "1" ]]; then
    query+="&insecure=1"
  else
    query+="&insecure=0"
  fi
  if [[ "$CFG_OBFS" == "1" ]]; then
    query+="&obfs=salamander&obfs-password=$(urlencode "$CFG_OBFS_PASS")"
  fi
  if [[ "$CFG_CERT_MODE" == "self" ]]; then
    pin="$(cert_sha256_pin || true)"
    if [[ -n "$pin" ]]; then
      query+="&pinSHA256=${pin}"
    fi
  fi
  printf 'hysteria2://%s@%s/?%s' "$auth_enc" "$hostport" "$query"
}

# 客户端 yaml 内容（stdout）
build_client_yaml() {
  local client_up="$CFG_BW_DOWN"
  local client_down="$CFG_BW_UP"
  local pin=""
  pin="$(cert_sha256_pin || true)"
  cat <<EOF
# Hysteria2 client — generated $(date -Iseconds 2>/dev/null || date)
# server up/down 已按方向对调写入客户端

server: $(yaml_quote "${CFG_SERVER_ADDR}:${CFG_LISTEN}")

auth: $(yaml_quote "$CFG_AUTH_PASS")

bandwidth:
  up: $(yaml_quote "$client_up")
  down: $(yaml_quote "$client_down")

tls:
  sni: $(yaml_quote "$CFG_SNI")
EOF
  if [[ "$CFG_INSECURE" == "1" ]]; then
    echo "  insecure: true"
  fi
  # 自签：写入 pinSHA256，客户端可校验证书指纹（防 MITM）
  if [[ -n "$pin" ]]; then
    echo "  pinSHA256: ${pin}"
  fi
  if [[ "$CFG_OBFS" == "1" ]]; then
    cat <<EOF

obfs:
  type: salamander
  salamander:
    password: $(yaml_quote "$CFG_OBFS_PASS")
EOF
  fi
  if [[ "$CFG_IS_RANGE" == "1" ]]; then
    cat <<EOF

transport:
  udp:
    hopInterval: $(yaml_quote "$CFG_HOP_INTERVAL")
EOF
  fi
}

# Surge 策略行（便于复制；字段名与 Surge 手册一致）
build_surge_line() {
  local pin port_part bw_num
  pin="$(cert_sha256_pin || true)"
  # 端口跳跃时第三段写起始端口，完整范围放在 port-hopping
  port_part="${CFG_LISTEN%%-*}"
  bw_num="${CFG_BW_UP%% *}"
  printf 'ProxyHY2 = hysteria2, %s, %s, password=%s, download-bandwidth=%s, sni=%s' \
    "$CFG_SERVER_ADDR" "$port_part" "$CFG_AUTH_PASS" "$bw_num" "$CFG_SNI"
  if [[ "$CFG_IS_RANGE" == "1" ]]; then
    printf ', port-hopping=%s, port-hopping-interval=%s' \
      "$CFG_LISTEN" "$(echo "$CFG_HOP_INTERVAL" | tr -cd '0-9')"
  fi
  if [[ "$CFG_INSECURE" == "1" ]]; then
    printf ', skip-cert-verify=true'
  fi
  if [[ -n "$pin" ]]; then
    printf ', server-cert-fingerprint-sha256=%s' "$pin"
  fi
  if [[ "$CFG_OBFS" == "1" ]]; then
    printf ', salamander-password=%s' "$CFG_OBFS_PASS"
  fi
  printf '\n'
}

# 人类可读摘要
build_summary() {
  local uri pin
  uri="$(build_share_uri)"
  pin="$(cert_sha256_pin || true)"
  cat <<EOF
========================================
 Hysteria2 连接信息
========================================
 生成时间 : $(date -Iseconds 2>/dev/null || date)
 服务地址 : ${CFG_SERVER_ADDR}
 监听端口 : ${CFG_LISTEN}$([[ "$CFG_IS_RANGE" == "1" ]] && echo " (端口跳跃)")
 认证密码 : ${CFG_AUTH_PASS}
 TLS SNI  : ${CFG_SNI}
 证书方式 : ${CFG_CERT_MODE}$([[ "$CFG_INSECURE" == "1" ]] && echo " (客户端需 skip-cert-verify / insecure)")
EOF
  if [[ -n "$pin" ]]; then
    cat <<EOF
 证书指纹 : ${pin}
            (pinSHA256 / Surge: server-cert-fingerprint-sha256)
            自签务必填写，避免只跳过校验被 MITM
EOF
  elif [[ "$CFG_CERT_MODE" == "self" ]]; then
    cat <<EOF
 证书指纹 : (未能读取 ${CERT_FILE}，请检查证书是否已生成)
EOF
  else
    cat <<EOF
 证书指纹 : (ACME 正式证书，一般无需填写 pin)
EOF
  fi
  cat <<EOF
 伪装模式 : ${CFG_MASQ_TYPE}$([[ "$CFG_MASQ_TCP" == "1" ]] && echo " + TCP:80/443")
 混淆     : $([[ "$CFG_OBFS" == "1" ]] && echo "salamander / ${CFG_OBFS_PASS}" || echo "关闭")
 配置取向 : 防 QoS 第一（偏低带宽 / 关激进抢速；非极限吞吐）
 带宽限制 : 服务端 up=${CFG_BW_UP}  down=${CFG_BW_DOWN}
            客户端 up=${CFG_BW_DOWN} down=${CFG_BW_UP}
 协议嗅探 : $([[ "$CFG_SNIFF" == "1" ]] && echo "开启" || echo "关闭")
 丢包补偿 : $([[ "$CFG_DISABLE_LOSS_COMP" == "1" ]] && echo "已关闭 (防 QoS)" || echo "开启 (易冲带宽，更易触发 QoS)")
 速度测试 : $([[ "$CFG_SPEEDTEST" == "1" ]] && echo "开启" || echo "关闭")
----------------------------------------
 分享 URI:
 ${uri}
----------------------------------------
 Surge 策略行（可直接粘贴到 [Proxy]）:
 $(build_surge_line | tr -d '\n')
----------------------------------------
 客户端配置文件: client.yaml
 使用: hysteria -c client.yaml client
========================================
EOF
}

export_connection_info() {
  local dir="$CFG_EXPORT_DIR"
  mkdir -p "$dir"
  local summary_file="${dir}/connection.txt"
  local client_file="${dir}/client.yaml"
  local uri_file="${dir}/share-uri.txt"
  local surge_file="${dir}/surge-proxy.txt"
  local uri
  uri="$(build_share_uri)"

  build_summary | tee "$summary_file"
  build_client_yaml >"$client_file"
  printf '%s\n' "$uri" >"$uri_file"
  build_surge_line >"$surge_file"

  # 同步一份到 /etc/hysteria，方便 root 本机查看
  mkdir -p "$CONFIG_DIR"
  cp -f "$summary_file" "${CONFIG_DIR}/connection.txt"
  cp -f "$client_file" "${CONFIG_DIR}/client.yaml"
  cp -f "$uri_file" "${CONFIG_DIR}/share-uri.txt"
  cp -f "$surge_file" "${CONFIG_DIR}/surge-proxy.txt"
  printf '%s\n' "$dir" >"$EXPORT_META"

  chmod 600 "$summary_file" "$client_file" "$uri_file" "$surge_file" \
    "${CONFIG_DIR}/connection.txt" "${CONFIG_DIR}/client.yaml" \
    "${CONFIG_DIR}/share-uri.txt" "${CONFIG_DIR}/surge-proxy.txt" \
    "$EXPORT_META" 2>/dev/null || true

  echo
  info "连接信息已保存到: ${dir}/"
  info "  - connection.txt   摘要（含 URI / 证书指纹 / Surge 行）"
  info "  - client.yaml      官方客户端配置（含 pinSHA256）"
  info "  - share-uri.txt    分享链接"
  info "  - surge-proxy.txt  Surge [Proxy] 策略行"
  info "副本: ${CONFIG_DIR}/connection.txt"
}

# 安装/重配完成后：启用开机自启并立刻启动
start_service() {
  systemctl daemon-reload
  systemctl enable --now "$SERVICE_NAME"
  sleep 1
  if systemctl is-active --quiet "$SERVICE_NAME"; then
    info "服务已启动（并已设置开机自启）: $SERVICE_NAME"
  else
    warn "服务未正常启动"
    diagnose_service_failure
    echo "  完整日志: journalctl -u $SERVICE_NAME -n 50 --no-pager"
    systemctl --no-pager --full status "$SERVICE_NAME" || true
  fi
}

require_service_unit() {
  if ! systemctl cat "$SERVICE_NAME" >/dev/null 2>&1; then
    err "未找到 systemd 服务 $SERVICE_NAME（请先安装）"
    return 1
  fi
  return 0
}

# 仅启动当前会话（不改动开机自启）
start_service_now() {
  title "启动服务"
  require_service_unit || return 1
  systemctl start "$SERVICE_NAME"
  sleep 1
  if systemctl is-active --quiet "$SERVICE_NAME"; then
    info "服务已启动: $SERVICE_NAME"
  else
    warn "启动失败"
    diagnose_service_failure
    echo "  完整日志: journalctl -u $SERVICE_NAME -n 50 --no-pager"
  fi
  systemctl --no-pager --full status "$SERVICE_NAME" || true
}

stop_service() {
  title "停止服务"
  require_service_unit || return 1
  systemctl stop "$SERVICE_NAME"
  sleep 1
  if systemctl is-active --quiet "$SERVICE_NAME"; then
    warn "服务仍在运行: $SERVICE_NAME"
  else
    info "服务已停止: $SERVICE_NAME"
  fi
  # 提示当前是否仍会开机自启
  if systemctl is-enabled --quiet "$SERVICE_NAME" 2>/dev/null; then
    warn "开机自启仍为开启（重启机器后会再次启动）。可选用「关闭开机自启」。"
  else
    info "开机自启: 已关闭"
  fi
  systemctl --no-pager --full status "$SERVICE_NAME" || true
}

enable_service() {
  title "开启开机自启"
  require_service_unit || return 1
  systemctl enable "$SERVICE_NAME"
  if systemctl is-enabled --quiet "$SERVICE_NAME" 2>/dev/null; then
    info "已设置开机自启: $SERVICE_NAME"
  else
    warn "设置开机自启可能失败，请手动检查: systemctl is-enabled $SERVICE_NAME"
  fi
  if prompt_yn "是否同时立即启动服务？" "y"; then
    systemctl start "$SERVICE_NAME"
    sleep 1
    if systemctl is-active --quiet "$SERVICE_NAME"; then
      info "服务已启动"
    else
      warn "启动失败，请检查日志"
    fi
  fi
  systemctl --no-pager --full status "$SERVICE_NAME" || true
}

disable_service() {
  title "关闭开机自启"
  require_service_unit || return 1
  systemctl disable "$SERVICE_NAME"
  if systemctl is-enabled --quiet "$SERVICE_NAME" 2>/dev/null; then
    warn "关闭开机自启可能失败"
  else
    info "已关闭开机自启: $SERVICE_NAME"
  fi
  if systemctl is-active --quiet "$SERVICE_NAME"; then
    if prompt_yn "服务当前仍在运行，是否一并停止？" "y"; then
      systemctl stop "$SERVICE_NAME"
      sleep 1
      if systemctl is-active --quiet "$SERVICE_NAME"; then
        warn "停止失败"
      else
        info "服务已停止"
      fi
    else
      info "服务保持运行；仅下次开机不再自动启动"
    fi
  fi
  systemctl --no-pager --full status "$SERVICE_NAME" || true
}

interactive_configure() {
  title "交互式配置 Hysteria2"

  # --- 端口（端口跳跃为可选项，默认单端口）---
  echo
  CFG_IS_RANGE=0
  echo "  端口模式："
  echo "  · 单端口（默认）：只开一个 UDP 端口，几乎所有环境可用"
  echo "  · 端口跳跃：客户端在端口范围内轮换，抗单端口限速；"
  echo "    需要本机可写 nft/iptables（完整 VPS；容器须 CAP_NET_ADMIN；rootless Podman 通常不行）"
  if prompt_yn "是否启用端口跳跃？" "n"; then
    echo "  范围格式: 起始-结束   示例: 20000-50000、10000-10100"
    local hop_range hop_start hop_end
    hop_range="$(prompt "端口跳跃范围" "$DEFAULT_HOP_RANGE")"
    hop_range="${hop_range// /}"
    if [[ ! "$hop_range" =~ ^([0-9]+)-([0-9]+)$ ]]; then
      err "范围格式须为 起始-结束，例如 20000-50000"
      exit 1
    fi
    hop_start="${BASH_REMATCH[1]}"
    hop_end="${BASH_REMATCH[2]}"
    if ((hop_start < 1 || hop_end > 65535 || hop_start > hop_end)); then
      err "范围无效：须满足 1 ≤ 起始 ≤ 结束 ≤ 65535"
      exit 1
    fi
    # 官方内置端口跳跃：listen 写成整段范围；服务端会调 iptables/nft 做 UDP 重定向
    ensure_firewall_for_porthop
    CFG_LISTEN="${hop_start}-${hop_end}"
    CFG_IS_RANGE=1
    CFG_HOP_INTERVAL="$(prompt "客户端跳端口间隔" "$DEFAULT_HOP_INTERVAL")"
    info "已启用端口跳跃: ${CFG_LISTEN}（间隔 ${CFG_HOP_INTERVAL}）"
  else
    CFG_LISTEN="$(prompt "监听端口" "$DEFAULT_LISTEN_PORT")"
    CFG_LISTEN="${CFG_LISTEN// /}"
    if [[ ! "$CFG_LISTEN" =~ ^[0-9]+$ ]] || ((CFG_LISTEN < 1 || CFG_LISTEN > 65535)); then
      err "端口须为 1-65535 的数字"
      exit 1
    fi
    info "使用单端口: ${CFG_LISTEN}"
  fi

  # --- 证书 ---
  echo
  echo "  1) ACME 自动证书（域名需解析到本机）"
  echo "  2) 自签证书（无域名可用）"
  local cert_choice
  cert_choice="$(prompt "证书方式" "2")"
  case "$cert_choice" in
    1)
      CFG_CERT_MODE="acme"
      CFG_DOMAIN="$(prompt "域名（已解析到本机）")"
      [[ -n "$CFG_DOMAIN" ]] || { err "域名不能为空"; exit 1; }
      CFG_EMAIL="$(prompt "ACME 邮箱" "admin@${CFG_DOMAIN}")"
      CFG_SNI="$CFG_DOMAIN"
      CFG_INSECURE=0
      ;;
    2|*)
      CFG_CERT_MODE="self"
      CFG_DOMAIN="$(prompt "证书 CN / 客户端 SNI（可用伪装站域名）" "www.bing.com")"
      CFG_SNI="$CFG_DOMAIN"
      CFG_INSECURE=1
      gen_self_signed_cert "$CFG_DOMAIN"
      ;;
  esac

  # --- 密码 ---
  CFG_AUTH_PASS="$(prompt "认证密码（回车随机）" "$(rand_password)")"

  # --- 伪装 ---
  echo
  echo "  1) 反代网站伪装（推荐）"
  echo "  2) 固定字符串响应"
  echo "  3) 不启用伪装"
  local masq_choice
  masq_choice="$(prompt "伪装模式" "1")"
  case "$masq_choice" in
    1)
      CFG_MASQ_TYPE="proxy"
      echo "  常用: https://www.bing.com/  https://www.microsoft.com/  https://www.apple.com/"
      CFG_MASQ_URL="$(prompt "伪装目标 URL" "$DEFAULT_MASQ_URL")"
      ;;
    2)
      CFG_MASQ_TYPE="string"
      CFG_MASQ_STRING="$(prompt "返回内容" "ok")"
      ;;
    3)
      CFG_MASQ_TYPE="none"
      ;;
    *)
      CFG_MASQ_TYPE="proxy"
      CFG_MASQ_URL="$(prompt "伪装目标 URL" "$DEFAULT_MASQ_URL")"
      ;;
  esac

  CFG_MASQ_TCP=0
  if [[ "$CFG_MASQ_TYPE" != "none" ]]; then
    echo
    info "TCP 80/443 伪装可模仿「网站同时提供 HTTP/HTTPS + HTTP/3」"
    warn "需确保本机 80/443 未被占用（如 nginx/caddy）"
    if prompt_yn "是否额外开启 TCP HTTP/HTTPS 伪装 (listenHTTP/HTTPS)？" "n"; then
      CFG_MASQ_TCP=1
    fi
  fi

  # --- 混淆 ---
  CFG_OBFS=0
  CFG_OBFS_PASS=""
  echo
  warn "Salamander 可绕过针对 QUIC/HTTP3 的封锁，但会失去正常网站外观。"
  if prompt_yn "是否启用 Salamander 混淆？" "n"; then
    CFG_OBFS=1
    CFG_OBFS_PASS="$(prompt "混淆密码" "$(rand_password)")"
  fi

  # --- 带宽 / 拥塞（防 QoS 第一）---
  echo
  title "带宽与拥塞行为（防 QoS 第一）"
  info "默认偏低带宽：故意不把线路打满，降低被运营商 QoS/限速盯上的概率"
  warn "拉满带宽往往更易触发 QoS；防 QoS 请保持默认，或只按「日常够用」小幅上调"
  CFG_BW_UP="$(prompt "服务器 up（≈客户端下载）" "$DEFAULT_BW_UP")"
  CFG_BW_DOWN="$(prompt "服务器 down（≈客户端上传）" "$DEFAULT_BW_DOWN")"
  CFG_DISABLE_LOSS_COMP=1
  echo
  info "Brutal 丢包补偿：丢包时更激进抢带宽 → 更容易顶满、更容易触发 QoS"
  if prompt_yn "关闭 Brutal 丢包补偿？（防 QoS，强烈推荐关）" "y"; then
    CFG_DISABLE_LOSS_COMP=1
  else
    CFG_DISABLE_LOSS_COMP=0
    warn "已开启丢包补偿：丢包时会更猛冲带宽，QoS 风险上升；被限速后请改回关闭"
  fi

  # --- 嗅探 / 测速 ---
  echo
  CFG_SNIFF=1
  if prompt_yn "启用协议嗅探 sniff？(TUN/ACL 推荐)" "y"; then
    CFG_SNIFF=1
  else
    CFG_SNIFF=0
  fi
  CFG_SPEEDTEST=0
  # 防 QoS：默认关内置测速，避免习惯性拉满测速引限速
  if prompt_yn "启用内置速度测试 speedTest？（易拉满测速，防 QoS 建议关）" "n"; then
    CFG_SPEEDTEST=1
  fi

  # --- 对外地址 ---
  local detected_ip
  detected_ip="$(detect_public_ip)"
  if [[ "$CFG_CERT_MODE" == "acme" ]]; then
    CFG_SERVER_ADDR="$(prompt "客户端连接地址（域名）" "$CFG_DOMAIN")"
  else
    CFG_SERVER_ADDR="$(prompt "客户端连接地址（IP 或域名）" "${detected_ip:-}")"
    [[ -n "$CFG_SERVER_ADDR" ]] || { err "连接地址不能为空"; exit 1; }
  fi

  # --- 导出目录（未填 / 直接回车 → 当前工作目录）---
  echo
  local last_export="" cwd_export
  cwd_export="$(pwd -P 2>/dev/null || pwd)"
  [[ -f "$EXPORT_META" ]] && last_export="$(tr -d '\r\n' <"$EXPORT_META" || true)"
  # 提示里优先展示上次目录，否则当前目录；真正为空时仍落回当前目录
  local export_hint="${last_export:-$cwd_export}"
  CFG_EXPORT_DIR="$(prompt "连接信息保存目录（直接回车=当前目录）" "$export_hint")"
  if [[ -z "$CFG_EXPORT_DIR" || "$CFG_EXPORT_DIR" == "." ]]; then
    CFG_EXPORT_DIR="$cwd_export"
  fi
  # 相对路径按当前目录展开，避免 root 下语义含糊
  if [[ "$CFG_EXPORT_DIR" != /* ]]; then
    CFG_EXPORT_DIR="${cwd_export}/${CFG_EXPORT_DIR}"
  fi

  # --- 写配置 & 启动 & 导出 ---
  write_server_config
  start_service

  # 稳优先：可选放大 UDP 缓冲（官方 Performance；可逆）
  maybe_apply_stability_tune

  title "连接信息"
  export_connection_info
}

show_status() {
  title "服务状态"
  if [[ -x "$HY2_BIN" ]]; then
    info "版本: $($HY2_BIN version 2>/dev/null | head -1 || echo unknown)"
  else
    warn "未安装 hysteria 二进制"
  fi
  if systemctl cat "$SERVICE_NAME" >/dev/null 2>&1; then
    local en_state act_state
    en_state="$(systemctl is-enabled "$SERVICE_NAME" 2>/dev/null || true)"
    act_state="$(systemctl is-active "$SERVICE_NAME" 2>/dev/null || true)"
    info "运行状态: ${act_state:-unknown}"
    info "开机自启: ${en_state:-unknown}"
    systemctl --no-pager --full status "$SERVICE_NAME" || true
  else
    warn "未找到 systemd 服务 $SERVICE_NAME"
  fi
  # 稳优先：状态里顺带看一眼 UDP 缓冲是否达标
  if is_linux; then
    local rmem wmem
    rmem="$(read_sysctl_int net.core.rmem_max)"
    wmem="$(read_sysctl_int net.core.wmem_max)"
    if [[ -n "$rmem" && -n "$wmem" ]] && ((rmem >= UDP_BUF_TARGET && wmem >= UDP_BUF_TARGET)); then
      info "UDP 缓冲: rmem=${rmem} wmem=${wmem}（已达标，有利于稳定）"
    elif [[ -n "$rmem" || -n "$wmem" ]]; then
      warn "UDP 缓冲: rmem=${rmem:-?} wmem=${wmem:-?}（<16MB，高负载易抖）→ sudo bash $SCRIPT_NAME tune"
    fi
  fi
}

resolve_export_dir() {
  if [[ -f "$EXPORT_META" ]]; then
    tr -d '\r\n' <"$EXPORT_META"
    return
  fi
  # 兼容：当前目录下有导出文件时
  local cwd
  cwd="$(pwd -P 2>/dev/null || pwd)"
  if [[ -f "${cwd}/connection.txt" ]]; then
    printf '%s' "$cwd"
    return
  fi
  printf ''
}

show_client() {
  title "连接信息"
  local dir
  dir="$(resolve_export_dir)"
  if [[ -n "$dir" && -f "${dir}/connection.txt" ]]; then
    info "导出目录: $dir"
    cat "${dir}/connection.txt"
    return
  fi
  if [[ -f "${CONFIG_DIR}/connection.txt" ]]; then
    info "来自 ${CONFIG_DIR}/connection.txt"
    cat "${CONFIG_DIR}/connection.txt"
    return
  fi
  if [[ -f "${CONFIG_DIR}/client.yaml" ]]; then
    cat "${CONFIG_DIR}/client.yaml"
    return
  fi
  err "尚未生成连接信息，请先安装/配置"
}

show_server_config() {
  title "服务端配置 $CONFIG_FILE"
  if [[ -f "$CONFIG_FILE" ]]; then
    cat "$CONFIG_FILE"
  else
    err "配置文件不存在"
  fi
}

restart_service() {
  title "重启服务"
  require_service_unit || return 1
  systemctl restart "$SERVICE_NAME"
  sleep 1
  if systemctl is-active --quiet "$SERVICE_NAME"; then
    info "服务已重启: $SERVICE_NAME"
  else
    warn "重启后未处于运行状态，请检查日志"
  fi
  systemctl --no-pager --full status "$SERVICE_NAME" || true
}

uninstall_all() {
  title "卸载"
  if ! prompt_yn "确认卸载 Hysteria2（官方 --remove）？" "n"; then
    info "已取消"
    return
  fi
  local keep_cfg=0
  if prompt_yn "是否保留配置与证书目录 $CONFIG_DIR？" "y"; then
    keep_cfg=1
  fi
  local dir
  dir="$(resolve_export_dir)"
  local keep_export=1
  if [[ -n "$dir" && -d "$dir" ]]; then
    if ! prompt_yn "是否保留连接信息目录 $dir？" "y"; then
      keep_export=0
    fi
  fi
  # 系统级 sysctl 与 hy2 二进制独立：默认一并撤销，避免残留
  if [[ -f "$SYSCTL_DROPIN" ]] || [[ -f "$SYSCTL_BACKUP" ]]; then
    if prompt_yn "是否同时撤销 UDP 缓冲区稳定性调优？" "y"; then
      revert_udp_buffer_tune || true
    else
      info "已保留 sysctl 调优（$SYSCTL_DROPIN）"
    fi
  fi
  bash <(curl -fsSL "$OFFICIAL_INSTALL_URL") --remove || true
  if [[ "$keep_cfg" -eq 0 ]]; then
    rm -rf "$CONFIG_DIR"
    info "已删除 $CONFIG_DIR"
  else
    info "已保留 $CONFIG_DIR"
  fi
  if [[ "$keep_export" -eq 0 && -n "$dir" ]]; then
    rm -rf "$dir"
    info "已删除 $dir"
  fi
}

full_install() {
  need_root
  ensure_deps
  install_binary
  interactive_configure
  title "完成"
  info "管理: sudo bash $SCRIPT_NAME status|start|stop|restart|enable|disable"
  info "或: systemctl status|start|stop|restart|enable|disable $SERVICE_NAME"
  info "日志: journalctl -u $SERVICE_NAME -f"
  info "再次查看连接: sudo bash $SCRIPT_NAME client"
  info "稳定性调优: sudo bash $SCRIPT_NAME tune|tune-status|untune"
}

main_menu() {
  need_root
  while true; do
    echo
    echo -e "${C_BOLD}========== Hysteria2 管理 ==========${C_RESET}"
    echo "  （取向：防 QoS 第一 · 少冲带宽 · 非极限吞吐）"
    echo "  1) 安装 / 升级（官方脚本）并配置"
    echo "  2) 仅重新配置（已安装二进制）"
    echo "  3) 查看连接信息（摘要 / URI）"
    echo "  4) 查看服务端配置"
    echo "  5) 服务状态"
    echo "  6) 启动服务"
    echo "  7) 停止服务"
    echo "  8) 重启服务"
    echo "  9) 开启开机自启"
    echo " 10) 关闭开机自启"
    echo " 11) 系统缓冲调优（UDP · 减丢包）"
    echo " 12) 卸载"
    echo "  0) 退出"
    echo -e "${C_BOLD}====================================${C_RESET}"
    local choice
    choice="$(prompt "请选择" "1")"
    case "$choice" in
      1) full_install ;;
      2)
        ensure_deps
        if [[ ! -x "$HY2_BIN" ]]; then
          err "未检测到 $HY2_BIN，请先选 1 安装"
          continue
        fi
        interactive_configure
        ;;
      3) show_client ;;
      4) show_server_config ;;
      5) show_status ;;
      6) start_service_now ;;
      7) stop_service ;;
      8) restart_service ;;
      9) enable_service ;;
      10) disable_service ;;
      11) stability_tune_menu ;;
      12) uninstall_all ;;
      0) exit 0 ;;
      *) warn "无效选项" ;;
    esac
  done
}

usage() {
  cat <<EOF
用法: sudo bash $SCRIPT_NAME [命令]

无参数进入交互菜单。

命令:
  install     官方安装 + 交互配置
  config      仅交互重新配置
  client      显示连接信息（摘要 / URI）
  status      服务状态
  start       启动服务（不改开机自启）
  stop        停止服务（不改开机自启）
  restart     重启服务
  enable      开启开机自启（可询问是否立即启动）
  disable     关闭开机自启（可询问是否同时停止）
  tune        应用 UDP 缓冲区稳定性调优（官方 Performance 16MB）
  tune-status 查看缓冲调优状态
  untune      撤销缓冲调优
  uninstall   卸载
  help        帮助

原则: 防 QoS 第一 > 连接稳定 > 极限吞吐
  · 偏低带宽 + 默认关 Brutal 丢包补偿（少打满、少冲带宽）
  · 伪装 / 可选端口跳跃 / 可选混淆（协议形态，视封锁情况）
  · 安装/重配默认应用 UDP 缓冲 16MB（减内核丢包；不替代防 QoS）
  · 不改 quic 流控窗口、不设 CPU 实时优先级

连接信息默认保存到: 当前工作目录（可交互自定义）
  connection.txt   摘要 + 分享 URI + 证书指纹 + Surge 行
  client.yaml      客户端配置（含 pinSHA256）
  share-uri.txt    仅 URI
  surge-proxy.txt  Surge [Proxy] 策略行
EOF
}

main() {
  local cmd="${1:-menu}"
  case "$cmd" in
    menu)      main_menu ;;
    install)   full_install ;;
    config)    need_root; ensure_deps; interactive_configure ;;
    client)    show_client ;;
    status)    need_root; show_status ;;
    start)     need_root; start_service_now ;;
    stop)      need_root; stop_service ;;
    restart)   need_root; restart_service ;;
    enable)    need_root; enable_service ;;
    disable)   need_root; disable_service ;;
    tune)      need_root; apply_udp_buffer_tune ;;
    tune-status) need_root; udp_buffer_status ;;
    untune)    need_root; revert_udp_buffer_tune ;;
    uninstall) need_root; uninstall_all ;;
    help|-h|--help) usage ;;
    *) usage; exit 1 ;;
  esac
}

main "$@"
