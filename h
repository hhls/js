#!/usr/bin/env bash
set -euo pipefail

SCRIPT_NAME="$(basename "$0")"
CONFIG_DIR="/etc/hysteria"
CONFIG_FILE="${CONFIG_DIR}/config.yaml"
CERT_FILE="${CONFIG_DIR}/server.crt"
KEY_FILE="${CONFIG_DIR}/server.key"
EXPORT_META="${CONFIG_DIR}/export-dir"
SERVICE_NAME="hysteria-server.service"
HY2_BIN="/usr/local/bin/hysteria"
OFFICIAL_INSTALL_URL="https://get.hy2.sh/"

SYSCTL_DROPIN="/etc/sysctl.d/99-hysteria-udp-buffer.conf"
SYSCTL_BACKUP="${CONFIG_DIR}/sysctl-udp-buffer.prev"
UDP_BUF_TARGET=16777216

DEFAULT_BW_UP="100 mbps"
DEFAULT_BW_DOWN="30 mbps"
DEFAULT_HOP_RANGE="20000-50000"
DEFAULT_LISTEN_PORT="443"
DEFAULT_MASQ_URL="https://www.bing.com/"
DEFAULT_HOP_INTERVAL="30s"

CFG_LISTEN=""
CFG_IS_RANGE=0
CFG_HOP_INTERVAL="$DEFAULT_HOP_INTERVAL"
CFG_CERT_MODE="self"
CFG_DOMAIN=""
CFG_EMAIL=""
CFG_SNI=""
CFG_INSECURE=1
CFG_AUTH_PASS=""
CFG_MASQ_TYPE="proxy"
CFG_MASQ_URL="$DEFAULT_MASQ_URL"
CFG_MASQ_STRING="ok"
CFG_MASQ_TCP=0
CFG_OBFS=0
CFG_OBFS_PASS=""
CFG_BW_UP="$DEFAULT_BW_UP"
CFG_BW_DOWN="$DEFAULT_BW_DOWN"
CFG_DISABLE_LOSS_COMP=1
CFG_SNIFF=0
CFG_SPEEDTEST=0
CFG_SERVER_ADDR=""
CFG_EXPORT_DIR=""

if [[ -t 1 ]] && command -v tput >/dev/null 2>&1; then
  C_RED=$(tput setaf 1)
  C_GREEN=$(tput setaf 2)
  C_YELLOW=$(tput setaf 3)
  C_BLUE=$(tput setaf 4)
  C_BOLD=$(tput bold)
  C_RESET=$(tput sgr0)
else
  C_RED='' C_GREEN='' C_YELLOW='' C_BLUE='' C_BOLD='' C_RESET=''
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
    read -r -p "$(echo -e "${C_BOLD}${msg}${C_RESET} [${def}]: ")" ans || { err "输入结束，已取消当前操作"; return 2; }
    ans="${ans:-$def}"
  else
    read -r -p "$(echo -e "${C_BOLD}${msg}${C_RESET}: ")" ans || { err "输入结束，已取消当前操作"; return 2; }
  fi
  printf '%s' "$ans"
}

prompt_yn() {
  local msg="$1"
  local def="${2:-y}"
  local ans
  while true; do
    ans="$(prompt "$msg (y/n)" "$def")" || return 2
    case "$ans" in
      [yY]|[yY][eE][sS]) return 0 ;;
      [nN]|[nN][oO])  return 1 ;;
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

yaml_quote() {
  local s="${1//\\/\\\\}"
  s="${s//\"/\\\"}"
  printf '"%s"' "$s"
}

urlencode() {
  local s="$1"
  if has_cmd python3; then
    python3 -c 'import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1], safe=""))' "$s"
    return
  fi
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
  for c in curl openssl python3; do
    has_cmd "$c" || missing+=("$c")
  done
  if ((${#missing[@]})); then
    pkg_install "${missing[@]}" || return 1
  fi
  for c in curl openssl python3; do
    has_cmd "$c" || { err "缺少依赖: $c"; return 1; }
  done
}


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
  echo "    1) 卸载重装时选「不启用端口跳跃」（单端口即可正常跑）"
  echo "    2) Docker/Podman rootful 示例："
  echo "       --cap-add=NET_ADMIN --network=host"
  echo "       （rootless Podman 一般仍会失败，见上方说明）"
  echo "    3) 换到完整 VPS / 宿主机再开端口跳跃"
  exit 1
}

diagnose_service_failure() {
  local log
  log="$(journalctl -u "$SERVICE_NAME" -n 30 --no-pager 2>/dev/null || true)"
  if echo "$log" | grep -qiE 'Operation not permitted|add table|nft |iptables'; then
    warn "日志像是「端口跳跃改防火墙失败」（nft/iptables 无权限）"
    echo "  配置里 listen 若是 起始-结束 范围，服务端会调 nft/iptables 做 UDP 重定向"
    echo "  当前环境没有 CAP_NET_ADMIN 时会启动失败"
    maybe_container_hint
    echo "  建议：卸载后重新安装，端口跳跃选 n（单端口）"
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


is_linux() { [[ "$(uname -s 2>/dev/null)" == "Linux" ]]; }

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

raise_sysctl_if_lower() {
  local key="$1" target="$2" cur
  cur="$(read_sysctl_int "$key")"
  if [[ -z "$cur" ]]; then
    warn "无法读取 ${key}，跳过"
    return 1
  fi
  if ((cur >= target)); then
    info "$key = ${cur}（已 ≥ ${target}，不降低）"
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
  echo "  目标值     : ${UDP_BUF_TARGET}"
  echo "  rmem_max   : ${rmem:-未知}"
  echo "  wmem_max   : ${wmem:-未知}"
  if [[ -f "$SYSCTL_DROPIN" ]]; then
    info "持久化文件: ${SYSCTL_DROPIN}（已安装；最终值取决于其它 sysctl 配置加载顺序）"
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
    warn "运行时缓冲未达标，可执行「应用稳定性调优」"
  fi
}

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
  if [[ ! -d "$(dirname "$SYSCTL_DROPIN")" ]]; then
    err "不存在 /etc/sysctl.d，无法持久化（可能是精简容器环境）"
    return 1
  fi

  local cur_r cur_w persist_r persist_w old_r old_w dropin_tmp backup_tmp
  cur_r="$(read_sysctl_int net.core.rmem_max)"
  cur_w="$(read_sysctl_int net.core.wmem_max)"
  if [[ -z "$cur_r" || -z "$cur_w" ]]; then
    err "无法读取当前 net.core.rmem_max / wmem_max"
    return 1
  fi

  if [[ ! -f "$SYSCTL_BACKUP" ]]; then
    mkdir -p "$CONFIG_DIR" || return 1
    backup_tmp="$(mktemp "${SYSCTL_BACKUP}.XXXXXX")" || return 1
    if ! printf 'net.core.rmem_max=%s\nnet.core.wmem_max=%s\n' "$cur_r" "$cur_w" >"$backup_tmp" ||
       ! chmod 644 "$backup_tmp" || ! mv -f "$backup_tmp" "$SYSCTL_BACKUP"; then
      rm -f "$backup_tmp"
      err "备份失败，未修改调优设置"
      return 1
    fi
    info "已备份当前值到 $SYSCTL_BACKUP"
  else
    info "已有备份 ${SYSCTL_BACKUP}（保留首次备份，不覆盖）"
  fi

  old_r="$(awk '$1 == "net.core.rmem_max" && $2 == "=" {print $3}' "$SYSCTL_DROPIN" 2>/dev/null | tail -1 || true)"
  old_w="$(awk '$1 == "net.core.wmem_max" && $2 == "=" {print $3}' "$SYSCTL_DROPIN" 2>/dev/null | tail -1 || true)"
  persist_r=$((cur_r > UDP_BUF_TARGET ? cur_r : UDP_BUF_TARGET))
  persist_w=$((cur_w > UDP_BUF_TARGET ? cur_w : UDP_BUF_TARGET))
  if [[ "$old_r" =~ ^[0-9]+$ ]] && ((old_r > persist_r)); then persist_r=$old_r; fi
  if [[ "$old_w" =~ ^[0-9]+$ ]] && ((old_w > persist_w)); then persist_w=$old_w; fi
  dropin_tmp="$(mktemp "${SYSCTL_DROPIN}.XXXXXX")" || return 1
  if ! printf 'net.core.rmem_max = %s\nnet.core.wmem_max = %s\n' "$persist_r" "$persist_w" >"$dropin_tmp" ||
     ! chmod 644 "$dropin_tmp" || ! mv -f "$dropin_tmp" "$SYSCTL_DROPIN"; then
    rm -f "$dropin_tmp"
    err "持久化失败，未修改运行时参数"
    return 1
  fi
  info "已写入 $SYSCTL_DROPIN"

  local ok=0
  raise_sysctl_if_lower net.core.rmem_max "$persist_r" && ok=$((ok + 1)) || true
  raise_sysctl_if_lower net.core.wmem_max "$persist_w" && ok=$((ok + 1)) || true
  if ((ok < 2)); then
    warn "运行时应用未完全成功；持久化文件已写，重启后可能仍会加载"
    warn "容器/无特权环境可能无法改 sysctl（需特权或宿主机调优）"
    return 1
  fi
  info "稳定性调优已应用（rmem_max / wmem_max ≥ ${UDP_BUF_TARGET}）"
  return 0
}

revert_udp_buffer_tune() {
  title "撤销稳定性调优（UDP 缓冲区）"
  if ! is_linux; then
    err "仅支持 Linux"
    return 1
  fi
  local had=0
  if [[ -f "$SYSCTL_DROPIN" ]]; then
    rm -f "$SYSCTL_DROPIN" || return 1
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
    if ! has_cmd sysctl; then
      err "未找到 sysctl，保留恢复备份: $SYSCTL_BACKUP"
      return 1
    fi
    local failed=0
    sysctl -w "net.core.rmem_max=${rest_r}" >/dev/null 2>&1 || failed=1
    sysctl -w "net.core.wmem_max=${rest_w}" >/dev/null 2>&1 || failed=1
    if ((failed)); then
      warn "运行时未完全恢复，保留备份；重试: sudo bash $SCRIPT_NAME untune"
      return 1
    fi
    info "运行时已恢复: rmem_max=${rest_r} wmem_max=${rest_w}"
    rm -f "$SYSCTL_BACKUP" || return 1
    info "已删除备份 $SYSCTL_BACKUP"
    had=1
  else
    warn "无有效备份：仅移除了 drop-in；运行时数值保持不变，直到重启或其它配置覆盖"
    [[ ! -f "$SYSCTL_BACKUP" ]] || return 1
  fi

  if ((had == 0)); then
    info "无需撤销（未应用过）"
  else
    info "撤销完成"
  fi
  return 0
}

maybe_apply_stability_tune() {
  if ! is_linux; then
    return 0
  fi
  echo
  title "系统缓冲调优"
  local rmem wmem
  rmem="$(read_sysctl_int net.core.rmem_max)"
  wmem="$(read_sysctl_int net.core.wmem_max)"
  if [[ -n "$rmem" && -n "$wmem" ]] && ((rmem >= UDP_BUF_TARGET && wmem >= UDP_BUF_TARGET)); then
    info "缓冲已达标（rmem=${rmem} wmem=${wmem}）"
    if [[ ! -f "$SYSCTL_DROPIN" ]] || ! awk -v r="$rmem" -v w="$wmem" '$1 == "net.core.rmem_max" && $3 >= r {a=1} $1 == "net.core.wmem_max" && $3 >= w {b=1} END {exit !(a && b)}' "$SYSCTL_DROPIN"; then
      info "同步当前较高值到持久化文件: $SYSCTL_DROPIN"
      apply_udp_buffer_tune || true
    fi
    return 0
  fi
  if prompt_yn "应用 UDP 缓冲区调优？" "y"; then
    apply_udp_buffer_tune || warn "调优未完全成功，可稍后: sudo bash $SCRIPT_NAME tune"
  else
    [[ $? == 1 ]] || return 1
    warn "已跳过缓冲调优"
    info "需要时: sudo bash $SCRIPT_NAME tune"
  fi
}




valid_cert_pair() {
  local cert_pub key_pub
  [[ -r "$CERT_FILE" && -r "$KEY_FILE" ]] || return 1
  openssl x509 -in "$CERT_FILE" -noout -checkend 0 >/dev/null 2>&1 || return 1
  cert_pub="$(openssl x509 -in "$CERT_FILE" -pubkey -noout 2>/dev/null)" || return 1
  key_pub="$(openssl pkey -in "$KEY_FILE" -passin pass: -pubout 2>/dev/null)" || return 1
  [[ -n "$cert_pub" && "$cert_pub" == "$key_pub" ]]
}

format_host() {
  local host="$1"
  if [[ "$host" == *:* && "$host" != \[*\] ]]; then
    printf '[%s]' "$host"
  else
    printf '%s' "$host"
  fi
}

validate_bandwidth() {
  [[ -z "$1" || "$1" =~ ^[0-9]+([.][0-9]+)?[[:space:]]*(bps|b|kbps|kb|k|mbps|mb|m|gbps|gb|g|tbps|tb|t)$ ]]
}

normalize_hop_interval() {
  local value="$1" number unit factor=1
  [[ "$value" =~ ^([0-9]{1,8})(s|m|h)$ ]] || return 1
  number=$((10#${BASH_REMATCH[1]}))
  unit="${BASH_REMATCH[2]}"
  case "$unit" in m) factor=60 ;; h) factor=3600 ;; esac
  number=$((number * factor))
  ((number >= 5 && number <= 2147483647)) || return 1
  printf '%ss' "$number"
}

validate_connection_inputs() {
  if ! has_cmd python3; then
    err "缺少依赖: python3"
    return 1
  fi
  [[ "$CFG_BW_UP" != none ]] || CFG_BW_UP=""
  [[ "$CFG_BW_DOWN" != none ]] || CFG_BW_DOWN=""
  if ! validate_bandwidth "$CFG_BW_UP" || ! validate_bandwidth "$CFG_BW_DOWN"; then
    err "带宽格式无效，例如 100 mbps；none 表示不设置该方向限制"
    return 1
  fi
  if [[ "$CFG_IS_RANGE" == "1" ]]; then
    CFG_HOP_INTERVAL="$(normalize_hop_interval "$CFG_HOP_INTERVAL")" || {
      err "跳跃间隔须为整数 s/m/h，至少 5 秒，例如 30s"
      return 1
    }
  fi
  CFG_SERVER_ADDR="${CFG_SERVER_ADDR#[}"
  CFG_SERVER_ADDR="${CFG_SERVER_ADDR%]}"
  [[ -n "$CFG_SERVER_ADDR" && "$CFG_SERVER_ADDR" != *[[:space:]/@?#%]* ]] || {
    err "连接地址须为域名或 IP，不含端口、URL 或 zone ID"
    return 1
  }
  if [[ "$CFG_SERVER_ADDR" == *:* ]]; then
    python3 -c 'import ipaddress,sys; ipaddress.IPv6Address(sys.argv[1])' "$CFG_SERVER_ADDR" >/dev/null 2>&1 || {
      err "IPv6 地址无效（连接端口另行填写）"
      return 1
    }
  fi
}


commit_config() {
  local candidate="$1"
  chmod 600 "$candidate" || return 1
  fix_owner "$candidate"
  mv -f "$candidate" "$CONFIG_FILE" || return 1
}



run_official_installer() {
  local installer status=0
  installer="$(mktemp)" || return 1
  if ! curl -fsSL "$OFFICIAL_INSTALL_URL" -o "$installer" || [[ ! -s "$installer" ]]; then
    rm -f "$installer"
    err "官方安装器下载失败，未执行安装/卸载"
    return 1
  fi
  bash "$installer" "$@" || status=$?
  rm -f "$installer"
  return "$status"
}

install_binary() {
  title "安装 / 升级"
  run_official_installer || return 1
  if [[ ! -x "$HY2_BIN" ]]; then
    err "安装失败：找不到 $HY2_BIN"
    exit 1
  fi
  info "安装完成: $($HY2_BIN version 2>/dev/null | head -1 || true)"
}

gen_self_signed_cert() {
  local cn="$1" cert_stage key_stage
  mkdir -p "$CONFIG_DIR"
  cert_stage="$(mktemp "${CONFIG_DIR}/.cert.XXXXXX")" || return 1
  key_stage="$(mktemp "${CONFIG_DIR}/.key.XXXXXX")" || { rm -f "$cert_stage"; return 1; }
  if ! openssl req -x509 -nodes -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 \
    -keyout "$key_stage" -out "$cert_stage" \
    -subj "/CN=${cn}" -days 3650 >/dev/null 2>&1; then
    rm -f "$cert_stage" "$key_stage"
    err "证书生成失败"
    return 1
  fi
  chmod 600 "$key_stage"
  chmod 644 "$cert_stage"
  fix_owner "$cert_stage" "$key_stage"
  mv -f "$key_stage" "$KEY_FILE"
  mv -f "$cert_stage" "$CERT_FILE"
  info "已生成自签证书 CN=${cn}"
}

write_server_config() {
  mkdir -p "$CONFIG_DIR"
  local candidate
  candidate="$(mktemp "${CONFIG_DIR}/.config.XXXXXX")" || return 1
  {
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
EOF
    [[ -z "$CFG_BW_UP" ]] || echo "  up: $(yaml_quote "$CFG_BW_UP")"
    [[ -z "$CFG_BW_DOWN" ]] || echo "  down: $(yaml_quote "$CFG_BW_DOWN")"
    if [[ "$CFG_DISABLE_LOSS_COMP" == "1" ]]; then
      echo "  disableLossCompensation: true"
    fi

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
  } >"$candidate"

  commit_config "$candidate" || return 1
  info "已写入服务端配置: $CONFIG_FILE"
}

build_share_uri() {
  local auth_enc hostport query pin
  auth_enc="$(urlencode "$CFG_AUTH_PASS")"
  hostport="$(format_host "$CFG_SERVER_ADDR"):${CFG_LISTEN}"
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

build_client_yaml() {
  local client_up="$CFG_BW_DOWN"
  local client_down="$CFG_BW_UP"
  local pin=""
  pin="$(cert_sha256_pin || true)"
  cat <<EOF

server: $(yaml_quote "$(format_host "$CFG_SERVER_ADDR"):${CFG_LISTEN}")

auth: $(yaml_quote "$CFG_AUTH_PASS")

socks5:
  listen: 127.0.0.1:1080

bandwidth:
EOF
  [[ -z "$client_up" ]] || echo "  up: $(yaml_quote "$client_up")"
  [[ -z "$client_down" ]] || echo "  down: $(yaml_quote "$client_down")"
  if [[ -z "$client_up" && -z "$client_down" ]]; then echo "  {}"; fi
  cat <<EOF

tls:
  sni: $(yaml_quote "$CFG_SNI")
EOF
  if [[ "$CFG_INSECURE" == "1" ]]; then
    echo "  insecure: true"
  else
    echo "  insecure: false"
  fi
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

build_surge_line() {
  local pin port_part bw_num
  pin="$(cert_sha256_pin || true)"
  port_part="${CFG_LISTEN%%-*}"
  bw_num="$(python3 - "$CFG_BW_UP" <<'PYBW'
import re, sys
from decimal import Decimal
m = re.fullmatch(r'([0-9]+(?:\.[0-9]+)?)\s*(bps|b|kbps|kb|k|mbps|mb|m|gbps|gb|g|tbps|tb|t)', sys.argv[1])
if m:
    scale = {'b': 0, 'k': 3, 'm': 6, 'g': 9, 't': 12}[m[2][0]]
    value = Decimal(m[1]) * Decimal(10) ** (scale - 6)
    if value:
        print(format(value, 'f'))
PYBW
)" || return 1
  printf 'Proxy = hysteria2, %s, %s, password=%s, sni=%s' \
    "$CFG_SERVER_ADDR" "$port_part" "$CFG_AUTH_PASS" "$CFG_SNI"
  if [[ -n "$bw_num" && "$bw_num" != "0" ]]; then
    printf ', download-bandwidth=%s' "$bw_num"
  fi
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

build_summary() {
  local uri pin surge
  surge="$(build_surge_line)" || return 1
  uri="$(build_share_uri)" || return 1
  pin="$(cert_sha256_pin || true)"
  cat <<EOF
========================================
 连接信息
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
 混淆     : $([[ "$CFG_OBFS" == "1" ]] && echo "开启 / ${CFG_OBFS_PASS}" || echo "关闭")
 每客户端限制 : 服务端 up=${CFG_BW_UP}  down=${CFG_BW_DOWN}
            客户端 up=${CFG_BW_DOWN} down=${CFG_BW_UP}
 协议嗅探 : $([[ "$CFG_SNIFF" == "1" ]] && echo "开启" || echo "关闭")
 服务端发送丢包补偿 : $([[ "$CFG_DISABLE_LOSS_COMP" == "1" ]] && echo "已关闭" || echo "开启")
 速度测试 : $([[ "$CFG_SPEEDTEST" == "1" ]] && echo "开启" || echo "关闭")
----------------------------------------
 分享 URI:
 ${uri}
----------------------------------------
 Surge 策略行（可直接粘贴到 [Proxy]）:
 ${surge}
----------------------------------------
 客户端配置文件: client.yaml
 使用: hysteria -c client.yaml client
========================================
EOF
}

export_connection_info() (
  umask 077
  local dir="$CFG_EXPORT_DIR" stage="" copy_stage="" meta_candidate="" file
  mkdir -p "$dir" "$CONFIG_DIR" || return 1
  stage="$(mktemp -d "${dir}/.hy2-export.XXXXXX")" || return 1
  trap 'rm -rf -- "$stage"; if [[ -n "$copy_stage" ]]; then rm -rf -- "$copy_stage"; fi; if [[ -n "$meta_candidate" ]]; then rm -f -- "$meta_candidate"; fi' EXIT

  build_summary >"$stage/connection.txt" || return 1
  build_client_yaml >"$stage/client.yaml" || return 1
  build_share_uri >"$stage/share-uri.txt" || return 1
  printf '\n' >>"$stage/share-uri.txt" || return 1
  build_surge_line >"$stage/surge-proxy.txt" || return 1

  if [[ ! "$dir" -ef "$CONFIG_DIR" ]]; then
    copy_stage="$(mktemp -d "${CONFIG_DIR}/.hy2-export.XXXXXX")" || return 1
    for file in connection.txt client.yaml share-uri.txt surge-proxy.txt; do
      cp "$stage/$file" "$copy_stage/$file" || return 1
    done
  fi
  meta_candidate="$(mktemp "${EXPORT_META}.XXXXXX")" || return 1
  printf '%s\n' "$dir" >"$meta_candidate" || return 1
  [[ ! -d "$EXPORT_META" ]] || { err "导出目录记录路径是目录: $EXPORT_META"; return 1; }

  for file in connection.txt client.yaml share-uri.txt surge-proxy.txt; do
    [[ ! -d "$dir/$file" && ! -d "$CONFIG_DIR/$file" ]] || {
      err "导出目标是目录: $file"
      return 1
    }
    chmod 600 "$stage/$file" || return 1
    if [[ -n "$copy_stage" ]]; then chmod 600 "$copy_stage/$file" || return 1; fi
  done
  for file in connection.txt client.yaml share-uri.txt surge-proxy.txt; do
    mv -f -- "$stage/$file" "$dir/$file" || return 1
    if [[ -n "$copy_stage" ]]; then
      mv -f -- "$copy_stage/$file" "$CONFIG_DIR/$file" || return 1
    fi
  done
  mv -f -- "$meta_candidate" "$EXPORT_META" || return 1
  cat "$dir/connection.txt" || return 1

  echo
  info "连接信息已保存到: ${dir}/"
  info "  - connection.txt   摘要（含 URI / 证书指纹 / Surge 行）"
  info "  - client.yaml      官方客户端配置（含 pinSHA256）"
  info "  - share-uri.txt    分享链接"
  info "  - surge-proxy.txt  Surge [Proxy] 策略行"
  info "副本: ${CONFIG_DIR}/connection.txt"
)

start_service() {
  systemctl daemon-reload || return 1
  systemctl enable "$SERVICE_NAME" || return 1
  local restarted=0
  systemctl restart "$SERVICE_NAME" && restarted=1
  sleep 1
  if [[ "$restarted" == "1" ]] && systemctl is-active --quiet "$SERVICE_NAME"; then
    info "配置已加载，服务已重启（并已设置开机自启）: $SERVICE_NAME"
  else
    warn "服务未正常启动"
    diagnose_service_failure
    echo "  完整日志: journalctl -u $SERVICE_NAME -n 50 --no-pager"
    systemctl --no-pager --full status "$SERVICE_NAME" || true
    return 1
  fi
}






interactive_configure() {
  CERT_FILE="${CONFIG_DIR}/server.crt"
  KEY_FILE="${CONFIG_DIR}/server.key"
  title "交互式配置"

  echo
  CFG_IS_RANGE=0
  echo "  端口模式："
  echo "  · 单端口（默认）"
  echo "  · 端口跳跃：使用 UDP 端口范围"
  echo "    需要本机可写 nft/iptables（完整 VPS；容器须 CAP_NET_ADMIN；rootless Podman 通常不行）"
  if prompt_yn "是否启用端口跳跃？" "n"; then
    echo "  范围格式: 起始-结束   示例: 20000-50000、10000-10100"
    local hop_range hop_start hop_end
    hop_range="$(prompt "端口跳跃范围" "$DEFAULT_HOP_RANGE")" || return 1
    hop_range="${hop_range// /}"
    if [[ ! "$hop_range" =~ ^([0-9]{1,5})-([0-9]{1,5})$ ]]; then
      err "范围格式须为 起始-结束，例如 20000-50000"
      exit 1
    fi
    hop_start=$((10#${BASH_REMATCH[1]}))
    hop_end=$((10#${BASH_REMATCH[2]}))
    if ((hop_start < 1 || hop_end > 65535 || hop_start > hop_end)); then
      err "范围无效：须满足 1 ≤ 起始 ≤ 结束 ≤ 65535"
      exit 1
    fi
    ensure_firewall_for_porthop
    CFG_LISTEN="${hop_start}-${hop_end}"
    CFG_IS_RANGE=1
    CFG_HOP_INTERVAL="$(prompt "客户端跳端口间隔" "$DEFAULT_HOP_INTERVAL")" || return 1
    info "已启用端口跳跃: ${CFG_LISTEN}（间隔 ${CFG_HOP_INTERVAL}）"
    info "请在云厂商 Security List / NSG 和本机防火墙放行 UDP $CFG_LISTEN"
  else
    [[ $? == 1 ]] || return 1
    CFG_LISTEN="$(prompt "监听端口" "$DEFAULT_LISTEN_PORT")" || return 1
    CFG_LISTEN="${CFG_LISTEN// /}"
    if [[ ! "$CFG_LISTEN" =~ ^[0-9]{1,5}$ ]] || ((10#$CFG_LISTEN < 1 || 10#$CFG_LISTEN > 65535)); then
      err "端口须为 1-65535 的数字"
      exit 1
    fi
    CFG_LISTEN=$((10#$CFG_LISTEN))
    info "使用单端口: ${CFG_LISTEN}"
  fi

  echo
  echo "  1) ACME 自动证书（域名需解析到本机）"
  echo "  2) 自签证书（无域名可用）"
  local cert_choice
  cert_choice="$(prompt "证书方式" "2")" || return 1
  case "$cert_choice" in
    1)
      CFG_CERT_MODE="acme"
      CFG_DOMAIN="$(prompt "域名（已解析到本机）")" || return 1
      [[ -n "$CFG_DOMAIN" ]] || { err "域名不能为空"; exit 1; }
      CFG_EMAIL="$(prompt "ACME 邮箱" "admin@${CFG_DOMAIN}")" || return 1
      CFG_SNI="$CFG_DOMAIN"
      CFG_INSECURE=0
      ;;
    2|*)
      CFG_CERT_MODE="self"
      CFG_DOMAIN="$(prompt "证书 CN / 客户端 SNI（可用伪装站域名）" "www.bing.com")" || return 1
      CFG_SNI="$CFG_DOMAIN"
      CFG_INSECURE=1
      ;;
  esac

  CFG_AUTH_PASS="$(prompt "认证密码（回车随机）" "$(rand_password)")" || return 1

  echo
  echo "  1) 反代网站伪装（推荐）"
  echo "  2) 固定字符串响应"
  echo "  3) 不启用伪装"
  local masq_choice
  masq_choice="$(prompt "伪装模式" "1")" || return 1
  case "$masq_choice" in
    1)
      CFG_MASQ_TYPE="proxy"
      CFG_MASQ_URL="$(prompt "伪装目标 URL" "$DEFAULT_MASQ_URL")" || return 1
      ;;
    2)
      CFG_MASQ_TYPE="string"
      CFG_MASQ_STRING="$(prompt "返回内容" "ok")" || return 1
      ;;
    3)
      CFG_MASQ_TYPE="none"
      ;;
    *)
      CFG_MASQ_TYPE="proxy"
      CFG_MASQ_URL="$(prompt "伪装目标 URL" "$DEFAULT_MASQ_URL")" || return 1
      ;;
  esac

  CFG_MASQ_TCP=0
  if [[ "$CFG_MASQ_TYPE" != "none" ]]; then
    echo
    warn "需确保本机 80/443 未被占用（如 nginx/caddy）"
    if prompt_yn "是否额外开启 TCP HTTP/HTTPS 伪装 (listenHTTP/HTTPS)？" "n"; then
      CFG_MASQ_TCP=1
    else
      [[ $? == 1 ]] || return 1
    fi
  fi

  CFG_OBFS=0
  CFG_OBFS_PASS=""
  echo
  warn "启用混淆后，客户端须使用相同的混淆密码。"
  if prompt_yn "是否启用混淆？" "n"; then
    CFG_OBFS=1
    CFG_OBFS_PASS="$(prompt "混淆密码" "$(rand_password)")" || return 1
  else
    [[ $? == 1 ]] || return 1
  fi

  echo
  title "带宽与拥塞行为"
  CFG_BW_UP="$(prompt "服务器 up（≈客户端下载，none 不设限制）" "$DEFAULT_BW_UP")" || return 1
  CFG_BW_DOWN="$(prompt "服务器 down（≈客户端上传，none 不设限制）" "$DEFAULT_BW_DOWN")" || return 1
  CFG_DISABLE_LOSS_COMP=1
  echo
  if prompt_yn "关闭服务端发送方向丢包补偿？" "y"; then
    CFG_DISABLE_LOSS_COMP=1
  else
    [[ $? == 1 ]] || return 1
    CFG_DISABLE_LOSS_COMP=0
  fi

  echo
  CFG_SNIFF=0
  if prompt_yn "启用协议嗅探 sniff？（域名识别/ACL，默认关闭）" "n"; then
    CFG_SNIFF=1
  else
    [[ $? == 1 ]] || return 1
    CFG_SNIFF=0
  fi
  CFG_SPEEDTEST=0
  if prompt_yn "启用内置速度测试 speedTest？" "n"; then
    CFG_SPEEDTEST=1
  else
    [[ $? == 1 ]] || return 1
  fi

  local detected_ip
  detected_ip="$(detect_public_ip)"
  if [[ "$CFG_CERT_MODE" == "acme" ]]; then
    CFG_SERVER_ADDR="$(prompt "客户端连接地址（域名）" "$CFG_DOMAIN")" || return 1
  else
    CFG_SERVER_ADDR="$(prompt "客户端连接地址（IP 或域名）" "${detected_ip:-}")" || return 1
    [[ -n "$CFG_SERVER_ADDR" ]] || { err "连接地址不能为空"; exit 1; }
  fi

  echo
  local last_export="" cwd_export
  cwd_export="$(pwd -P 2>/dev/null || pwd)"
  [[ -f "$EXPORT_META" ]] && last_export="$(tr -d '\r\n' <"$EXPORT_META" || true)"
  local export_hint="${last_export:-$cwd_export}"
  CFG_EXPORT_DIR="$(prompt "连接信息保存目录（直接回车=当前目录）" "$export_hint")" || return 1
  if [[ -z "$CFG_EXPORT_DIR" || "$CFG_EXPORT_DIR" == "." ]]; then
    CFG_EXPORT_DIR="$cwd_export"
  fi
  if [[ "$CFG_EXPORT_DIR" != /* ]]; then
    CFG_EXPORT_DIR="${cwd_export}/${CFG_EXPORT_DIR}"
  fi

  validate_connection_inputs || return 1
  if [[ "$CFG_CERT_MODE" == "self" ]]; then
    if [[ -f "$CERT_FILE" || -f "$KEY_FILE" ]]; then
      if valid_cert_pair; then
        info "复用现有证书与密钥"
      else
        err "已有证书/密钥不完整或无效，请先修复"
        return 1
      fi
    else
      gen_self_signed_cert "$CFG_DOMAIN" || return 1
    fi
  fi
  write_server_config || return 1
  start_service || return 1

  maybe_apply_stability_tune || return 1

  title "连接信息"
  export_connection_info
}

show_status() {
  title "服务状态"
  if [[ -x "$HY2_BIN" ]]; then
    info "版本: $($HY2_BIN version 2>/dev/null | head -1 || echo unknown)"
  else
    warn "尚未安装服务程序"
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
  if is_linux; then
    local rmem wmem
    rmem="$(read_sysctl_int net.core.rmem_max)"
    wmem="$(read_sysctl_int net.core.wmem_max)"
    if [[ -n "$rmem" && -n "$wmem" ]] && ((rmem >= UDP_BUF_TARGET && wmem >= UDP_BUF_TARGET)); then
      info "UDP 缓冲: rmem=${rmem} wmem=${wmem}（已达标）"
    elif [[ -n "$rmem" || -n "$wmem" ]]; then
      warn "UDP 缓冲: rmem=${rmem:-?} wmem=${wmem:-?}（未达标）→ sudo bash $SCRIPT_NAME tune"
    fi
  fi
}

resolve_export_dir() {
  if [[ -f "$EXPORT_META" ]]; then
    tr -d '\r\n' <"$EXPORT_META"
    return
  fi
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



uninstall_all() {
  title "卸载"
  if prompt_yn "确认卸载服务程序？" "n"; then
    :
  else
    [[ $? == 1 ]] || return 1
    info "已取消"
    return 0
  fi
  local keep_cfg=0 tune_failed=0 undo_tune=0
  if prompt_yn "是否保留配置与证书目录 ${CONFIG_DIR}？" "n"; then
    keep_cfg=1
  else
    [[ $? == 1 ]] || return 1
  fi
  local dir
  dir="$(resolve_export_dir)"
  local keep_export=1
  if [[ -n "$dir" && -d "$dir" ]]; then
    if prompt_yn "是否保留 ${dir} 中的连接信息文件？" "n"; then
      :
    else
      [[ $? == 1 ]] || return 1
      keep_export=0
    fi
  fi
  if [[ -f "$SYSCTL_DROPIN" ]] || [[ -f "$SYSCTL_BACKUP" ]]; then
    if prompt_yn "是否同时撤销 UDP 缓冲区稳定性调优？" "y"; then
      undo_tune=1
    else
      [[ $? == 1 ]] || return 1
    fi
  fi
  if ! run_official_installer --remove; then
    err "官方卸载失败，保留配置与连接信息；请排查后重试"
    return 1
  fi
  if ((undo_tune)); then
    revert_udp_buffer_tune || tune_failed=1
  fi
  if [[ "$keep_cfg" -eq 0 ]]; then
    if [[ -f "$SYSCTL_BACKUP" ]]; then
      find "$CONFIG_DIR" -mindepth 1 -maxdepth 1 ! -name "${SYSCTL_BACKUP##*/}" -exec rm -rf -- {} + || return 1
      info "已清理配置，保留调优恢复备份: $SYSCTL_BACKUP"
    else
      rm -rf "$CONFIG_DIR" || return 1
      info "已删除 $CONFIG_DIR"
    fi
  else
    info "已保留 $CONFIG_DIR"
  fi
  if [[ "$keep_export" -eq 0 && -n "$dir" ]]; then
    rm -f -- "$dir/connection.txt" "$dir/client.yaml" "$dir/share-uri.txt" "$dir/surge-proxy.txt"
    info "已删除 $dir 中的连接信息文件（保留目录和其它文件）"
  fi
  return "$tune_failed"
}

full_install() {
  need_root
  if [[ -f "$CONFIG_FILE" || -x "$HY2_BIN" ]]; then
    err "已检测到安装或配置，请先执行 uninstall，再执行 install"
    return 1
  fi
  ensure_deps || return 1
  install_binary || return 1
  interactive_configure || return 1
  title "完成"
  info "查看连接: sudo bash $SCRIPT_NAME client"
  info "更换配置: sudo bash $SCRIPT_NAME uninstall，然后重新 install"
  info "日志: journalctl -u $SERVICE_NAME -f"
}

main_menu() {
  need_root
  while true; do
    echo
    echo "========== 配置管理 =========="
    echo "  1) 一键安装"
    echo "  2) 一键卸载"
    echo "  3) 查看连接信息"
    echo "  4) 服务状态"
    echo "  0) 退出"
    local choice
    choice="$(prompt "请选择" "0")" || return 0
    case "$choice" in
      1) full_install || warn "安装未完成" ;;
      2) uninstall_all || warn "卸载未完成" ;;
      3) show_client ;;
      4) show_status ;;
      0) return 0 ;;
      *) warn "无效选项" ;;
    esac
  done
}

usage() {
  cat <<EOF
用法: sudo bash $SCRIPT_NAME [install|uninstall|client|status|tune|untune|help]

无参数进入菜单。
  install     一键安装并按提示生成配置
  uninstall   一键卸载（默认清理配置与连接文件）
  client      查看连接信息
  status      查看服务状态
  tune        应用 UDP 缓冲调优
  untune      撤销 UDP 缓冲调优
  help        帮助

需要更新配置时，先卸载再重新安装。
连接信息默认保存到当前目录，可在安装时指定。
EOF
}

main() {
  local cmd="${1:-menu}"
  case "$cmd" in
    menu)      main_menu ;;
    install)   full_install ;;
    client)    show_client ;;
    status)    need_root; show_status ;;
    uninstall) need_root; uninstall_all ;;
    tune)      need_root; apply_udp_buffer_tune ;;
    untune)    need_root; revert_udp_buffer_tune ;;
    help|-h|--help) usage ;;
    *) usage; exit 1 ;;
  esac
}

main "$@"
