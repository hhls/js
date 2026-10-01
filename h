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
  for c in curl openssl; do
    has_cmd "$c" || missing+=("$c")
  done
  if ((${#missing[@]})); then
    pkg_install "${missing[@]}"
  fi
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
  echo "    1) 重新配置时选「不启用端口跳跃」（单端口即可正常跑）"
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

  local cur_r cur_w persist_r persist_w old_r old_w dropin_tmp
  cur_r="$(read_sysctl_int net.core.rmem_max)"
  cur_w="$(read_sysctl_int net.core.wmem_max)"
  if [[ -z "$cur_r" || -z "$cur_w" ]]; then
    err "无法读取当前 net.core.rmem_max / wmem_max"
    return 1
  fi

  if [[ ! -f "$SYSCTL_BACKUP" ]]; then
    mkdir -p "$CONFIG_DIR"
    cat >"$SYSCTL_BACKUP" <<EOF
net.core.rmem_max=${cur_r}
net.core.wmem_max=${cur_w}
EOF
    chmod 644 "$SYSCTL_BACKUP" 2>/dev/null || true
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
  cat >"$dropin_tmp" <<EOF
net.core.rmem_max = ${persist_r}
net.core.wmem_max = ${persist_w}
EOF
  chmod 644 "$dropin_tmp"
  mv -f "$dropin_tmp" "$SYSCTL_DROPIN"
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
    warn "已跳过缓冲调优"
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

ensure_yaml_deps() {
  if python3 -c 'import yaml' >/dev/null 2>&1; then
    return 0
  fi
  if has_cmd apk; then
    pkg_install python3 py3-yaml || return 1
  elif has_cmd dnf || has_cmd yum; then
    pkg_install python3 python3-pyyaml || return 1
  else
    pkg_install python3 python3-yaml || return 1
  fi
  python3 -c 'import yaml' >/dev/null 2>&1 || {
    err "需要 Python 3 与 PyYAML，请安装发行版的 YAML 包后重试"
    return 1
  }
}

config_yaml() {
  python3 - "$@" <<'PY'
import copy, os, re, sys
import yaml
class Loader(yaml.SafeLoader):
    pass
def mapping(loader, node, deep=False):
    loader.flatten_mapping(node)
    result = {}
    for k, v in node.value:
        key = loader.construct_object(k, deep=deep)
        if key in result:
            raise ValueError('duplicate YAML key')
        result[key] = loader.construct_object(v, deep=deep)
    return result
Loader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, mapping)
def load(path):
    with open(path, encoding='utf-8') as f:
        data = yaml.load(f, Loader=Loader)
    if not isinstance(data, dict):
        raise ValueError('configuration must be a mapping')
    return data
def text(value):
    if not isinstance(value, (str, int, float)) or isinstance(value, bool):
        raise ValueError('expected scalar string or number')
    value = str(value)
    if any(ord(c) < 32 for c in value):
        raise ValueError('control characters are not supported')
    return value
try:
    mode, path = sys.argv[1:3]
    data = load(path)
    if mode == 'validate':
        pass
    elif mode == 'snapshot':
        auth = data.get('auth', {})
        if auth.get('type') != 'password' or not isinstance(auth.get('password'), str) or not auth['password']:
            raise ValueError('interactive reconfiguration requires password authentication')
        if data.get('obfs', {}).get('type', 'salamander') != 'salamander':
            raise ValueError('unsupported obfuscation for client export; edit manually')
        listen = text(data.get('listen', ':443'))
        port = listen.rsplit(':', 1)[-1]
        if not re.fullmatch(r'[0-9]+(?:-[0-9]+)?', port):
            raise ValueError('unsupported listen format; edit manually')
        bind = listen[:-len(port)]
        client_path = sys.argv[3]
        client = load(client_path) if os.path.isfile(client_path) else {}
        server = text(client.get('server', ''))
        if server.startswith(('hysteria2://', 'hy2://', 'realm://')):
            raise ValueError('URI client configuration requires manual export')
        host = server.rsplit(':', 1)[0].strip('[]') if ':' in server else server
        bw = data.get('bandwidth') or {}
        tls = data.get('tls', {})
        acme = data.get('acme', {})
        if not tls and not acme:
            raise ValueError('TLS configuration missing; edit manually')
        values = {
            'CFG_LISTEN': port, 'CFG_BIND_PREFIX': bind or ':',
            'CFG_AUTH_PASS': auth['password'],
            'CFG_BW_UP': bw.get('up', ''), 'CFG_BW_DOWN': bw.get('down', ''),
            'CFG_DISABLE_LOSS_COMP': int(bool(bw.get('disableLossCompensation', False))),
            'CFG_SNIFF': int(bool((data.get('sniff') or {}).get('enable', False))),
            'CFG_SPEEDTEST': int(bool(data.get('speedTest', False))),
            'CFG_CERT_MODE': 'acme' if acme else 'self',
            'CFG_SNI': client.get('tls', {}).get('sni', (acme.get('domains') or [''])[0]),
            'CFG_INSECURE': int(bool(client.get('tls', {}).get('insecure', not bool(acme)))),
            'CERT_FILE': tls.get('cert', ''), 'KEY_FILE': tls.get('key', ''),
            'CFG_SERVER_ADDR': host,
            'CFG_HOP_INTERVAL': client.get('transport', {}).get('udp', {}).get('hopInterval', '30s'),
            'CFG_OBFS': int(bool(data.get('obfs'))),
            'CFG_OBFS_PASS': data.get('obfs', {}).get('salamander', {}).get('password', ''),
            'CFG_MASQ_TYPE': data.get('masquerade', {}).get('type', 'none'),
            'CFG_MASQ_TCP': int(bool(data.get('masquerade', {}).get('listenHTTP') or data.get('masquerade', {}).get('listenHTTPS'))),
        }
        for key, value in values.items():
            sys.stdout.buffer.write((key + '\0' + text(value) + '\0').encode())
    elif mode == 'client':
        old = load(sys.argv[3])
        def merge(dst, src):
            for key, value in src.items():
                if isinstance(value, dict) and isinstance(dst.get(key), dict):
                    merge(dst[key], value)
                else:
                    dst[key] = value
        for key in ('up', 'down'):
            if key not in data.get('bandwidth', {}):
                (old.get('bandwidth') or {}).pop(key, None)
        if 'obfs' not in data:
            old.pop('obfs', None)
        if 'pinSHA256' not in data.get('tls', {}):
            (old.get('tls') or {}).pop('pinSHA256', None)
        if 'transport' not in data:
            for key in ('hopInterval', 'minHopInterval', 'maxHopInterval'):
                ((old.get('transport') or {}).get('udp') or {}).pop(key, None)
        else:
            for key in ('minHopInterval', 'maxHopInterval'):
                ((old.get('transport') or {}).get('udp') or {}).pop(key, None)
        merge(old, data)
        yaml.safe_dump(old, sys.stdout, allow_unicode=True, sort_keys=False)
    elif mode == 'patch':
        data = copy.deepcopy(data)
        data['listen'] = os.environ['CFG_BIND_PREFIX'] + os.environ['CFG_LISTEN']
        bw = data.get('bandwidth')
        if bw is None:
            bw = data['bandwidth'] = {}
        for key, env in [('up', 'CFG_BW_UP'), ('down', 'CFG_BW_DOWN')]:
            if os.environ[env]:
                bw[key] = os.environ[env]
            else:
                bw.pop(key, None)
        bw['disableLossCompensation'] = os.environ['CFG_DISABLE_LOSS_COMP'] == '1'
        data['auth']['password'] = os.environ['CFG_AUTH_PASS']
        sniff = data.get('sniff')
        if sniff is None:
            sniff = data['sniff'] = {}
        sniff['enable'] = os.environ['CFG_SNIFF'] == '1'
        data['speedTest'] = os.environ['CFG_SPEEDTEST'] == '1'
        yaml.safe_dump(data, sys.stdout, allow_unicode=True, sort_keys=False)
    else:
        raise ValueError('unknown mode')
except Exception:
    print('配置解析失败：检查 YAML、重复键、认证/TLS 类型及客户端导出格式；原配置未被改写。', file=sys.stderr)
    sys.exit(1)
PY
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

backup_configuration() {
  CONFIG_BACKUP_DIR="$(mktemp -d "${CONFIG_DIR}/backup-$(date +%Y%m%d-%H%M%S).XXXXXX")" || return 1
  chmod 700 "$CONFIG_BACKUP_DIR" || return 1
  local file
  for file in "$CONFIG_FILE" "$CERT_FILE" "$KEY_FILE"; do
    if [[ -n "$file" && -f "$file" ]]; then
      case "$file" in
        "$CONFIG_FILE") cp -p "$file" "$CONFIG_BACKUP_DIR/config.yaml" || return 1 ;;
        "$CERT_FILE") cp -p "$file" "$CONFIG_BACKUP_DIR/server.crt" || return 1 ;;
        "$KEY_FILE") cp -p "$file" "$CONFIG_BACKUP_DIR/server.key" || return 1 ;;
      esac
    fi
  done
  info "配置与证书备份: $CONFIG_BACKUP_DIR"
}

commit_config() {
  local candidate="$1"
  if ! config_yaml validate "$candidate"; then
    rm -f "$candidate"
    return 1
  fi
  if [[ -f "$CONFIG_FILE" ]]; then
    backup_configuration || { rm -f "$candidate"; return 1; }
  fi
  chmod 600 "$candidate" || return 1
  fix_owner "$candidate"
  mv -f "$candidate" "$CONFIG_FILE" || return 1
}

apply_config_service() {
  if start_service; then
    return 0
  fi
  if [[ -n "${CONFIG_BACKUP_DIR:-}" && -f "$CONFIG_BACKUP_DIR/config.yaml" ]]; then
    local restore
    restore="$(mktemp "${CONFIG_DIR}/.restore.XXXXXX")" || return 1
    cp -p "$CONFIG_BACKUP_DIR/config.yaml" "$restore" || return 1
    mv -f "$restore" "$CONFIG_FILE" || return 1
    warn "新配置启动失败，已恢复旧配置；备份: $CONFIG_BACKUP_DIR"
    if systemctl restart "$SERVICE_NAME" && systemctl is-active --quiet "$SERVICE_NAME"; then
      info "旧配置服务已恢复"
    else
      err "旧配置服务也未能恢复，请检查 journalctl -u $SERVICE_NAME"
    fi
  fi
  return 1
}

reconfigure_existing() {
  ensure_yaml_deps || return 1
  CONFIG_BACKUP_DIR=""
  local snapshot name value candidate choice pass
  snapshot="$(mktemp "${CONFIG_DIR}/.snapshot.XXXXXX")" || return 1
  if ! config_yaml snapshot "$CONFIG_FILE" "${CONFIG_DIR}/client.yaml" >"$snapshot"; then
    rm -f "$snapshot"
    return 1
  fi
  while IFS= read -r -d '' name && IFS= read -r -d '' value; do
    case "$name" in
      CFG_*|CERT_FILE|KEY_FILE) printf -v "$name" '%s' "$value" ;;
    esac
  done <"$snapshot"
  rm -f "$snapshot"
  CFG_IS_RANGE=0
  [[ "$CFG_LISTEN" == *-* ]] && CFG_IS_RANGE=1
  info "复用现有证书、密码和高级配置；回车保留当前值"
  CFG_LISTEN="$(prompt "监听端口或范围" "$CFG_LISTEN")"
  local start end
  if [[ "$CFG_LISTEN" =~ ^([0-9]{1,5})(-([0-9]{1,5}))?$ ]]; then
    start=$((10#${BASH_REMATCH[1]}))
    end=$start
    [[ -z "${BASH_REMATCH[3]}" ]] || end=$((10#${BASH_REMATCH[3]}))
    ((start >= 1 && end <= 65535 && start <= end)) || { err "端口范围无效"; return 1; }
    CFG_IS_RANGE=0
    if [[ "$CFG_LISTEN" == *-* ]]; then
      CFG_IS_RANGE=1
      ensure_firewall_for_porthop
      CFG_HOP_INTERVAL="$(prompt "客户端跳端口间隔" "$CFG_HOP_INTERVAL")"
      info "请在云厂商 Security List / NSG 和本机防火墙放行 UDP $CFG_LISTEN"
    fi
  else
    err "端口须为 1-65535 或 起始-结束"
    return 1
  fi
  CFG_BW_UP="$(prompt "服务端 up（每客户端；none 不设限制）" "$CFG_BW_UP")"
  CFG_BW_DOWN="$(prompt "服务端 down（每客户端；none 不设限制）" "$CFG_BW_DOWN")"
  [[ "$CFG_BW_UP" != "none" ]] || CFG_BW_UP=""
  [[ "$CFG_BW_DOWN" != "none" ]] || CFG_BW_DOWN=""
  choice=n; [[ "$CFG_DISABLE_LOSS_COMP" == "1" ]] && choice=y
  CFG_DISABLE_LOSS_COMP=0
  if prompt_yn "关闭服务端发送方向丢包补偿？" "$choice"; then CFG_DISABLE_LOSS_COMP=1; fi
  choice=n; [[ "$CFG_SNIFF" == "1" ]] && choice=y
  CFG_SNIFF=0
  if prompt_yn "启用 sniff（域名识别/ACL）？" "$choice"; then CFG_SNIFF=1; fi
  choice=n; [[ "$CFG_SPEEDTEST" == "1" ]] && choice=y
  CFG_SPEEDTEST=0
  if prompt_yn "启用内置测速？" "$choice"; then CFG_SPEEDTEST=1; fi
  if prompt_yn "更换认证密码（现有客户端须更新）？" "n"; then
    pass="$(prompt "新认证密码（回车随机）" "$(rand_password)")"
    CFG_AUTH_PASS="$pass"
  fi
  CFG_SERVER_ADDR="$(prompt "客户端连接地址（IP 或域名）" "${CFG_SERVER_ADDR:-$(detect_public_ip)}")"
  if [[ -z "$CFG_SNI" && -r "$CERT_FILE" ]]; then
    CFG_SNI="$(openssl x509 -in "$CERT_FILE" -noout -subject -nameopt multiline 2>/dev/null | sed -n 's/^[[:space:]]*commonName[[:space:]]*=[[:space:]]*//p')"
  fi
  CFG_SNI="$(prompt "客户端 SNI" "$CFG_SNI")"
  if [[ "$CFG_CERT_MODE" == "self" ]]; then
    if ! valid_cert_pair; then
      err "现有 TLS 证书/密钥无效、不匹配或已过期，请先修复；不会自动轮换证书"
      return 1
    fi
  fi
  validate_connection_inputs || return 1
  CFG_EXPORT_DIR="$(prompt "连接信息保存目录" "$(resolve_export_dir)")"
  CFG_EXPORT_DIR="${CFG_EXPORT_DIR:-$(pwd -P)}"
  [[ "$CFG_EXPORT_DIR" == /* ]] || CFG_EXPORT_DIR="$(pwd -P)/$CFG_EXPORT_DIR"
  candidate="$(mktemp "${CONFIG_DIR}/.config.XXXXXX")" || return 1
  if ! CFG_BIND_PREFIX="$CFG_BIND_PREFIX" CFG_LISTEN="$CFG_LISTEN" \
    CFG_BW_UP="$CFG_BW_UP" CFG_BW_DOWN="$CFG_BW_DOWN" \
    CFG_DISABLE_LOSS_COMP="$CFG_DISABLE_LOSS_COMP" CFG_AUTH_PASS="$CFG_AUTH_PASS" \
    CFG_SNIFF="$CFG_SNIFF" CFG_SPEEDTEST="$CFG_SPEEDTEST" \
    config_yaml patch "$CONFIG_FILE" >"$candidate"; then
    rm -f "$candidate"
    return 1
  fi
  commit_config "$candidate" || return 1
  apply_config_service || return 1
  maybe_apply_stability_tune
  export_connection_info
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
)"
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
  local uri pin
  uri="$(build_share_uri)"
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
 $(build_surge_line | tr -d '\n')
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
  if [[ -f "${CONFIG_DIR}/client.yaml" ]]; then
    config_yaml client "$stage/client.yaml" "${CONFIG_DIR}/client.yaml" >"$stage/client-merged.yaml" || return 1
    mv -f "$stage/client-merged.yaml" "$stage/client.yaml" || return 1
  fi
  config_yaml validate "$stage/client.yaml" || return 1
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

require_service_unit() {
  if ! systemctl cat "$SERVICE_NAME" >/dev/null 2>&1; then
    err "未找到 systemd 服务 ${SERVICE_NAME}（请先安装）"
    return 1
  fi
  return 0
}

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
  ensure_yaml_deps || return 1
  if [[ -f "$CONFIG_FILE" ]]; then
    reconfigure_existing
    return $?
  fi
  CONFIG_BACKUP_DIR=""
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
    hop_range="$(prompt "端口跳跃范围" "$DEFAULT_HOP_RANGE")"
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
    CFG_HOP_INTERVAL="$(prompt "客户端跳端口间隔" "$DEFAULT_HOP_INTERVAL")"
    info "已启用端口跳跃: ${CFG_LISTEN}（间隔 ${CFG_HOP_INTERVAL}）"
    info "请在云厂商 Security List / NSG 和本机防火墙放行 UDP $CFG_LISTEN"
  else
    CFG_LISTEN="$(prompt "监听端口" "$DEFAULT_LISTEN_PORT")"
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
      ;;
  esac

  CFG_AUTH_PASS="$(prompt "认证密码（回车随机）" "$(rand_password)")"

  echo
  echo "  1) 反代网站伪装（推荐）"
  echo "  2) 固定字符串响应"
  echo "  3) 不启用伪装"
  local masq_choice
  masq_choice="$(prompt "伪装模式" "1")"
  case "$masq_choice" in
    1)
      CFG_MASQ_TYPE="proxy"
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
    warn "需确保本机 80/443 未被占用（如 nginx/caddy）"
    if prompt_yn "是否额外开启 TCP HTTP/HTTPS 伪装 (listenHTTP/HTTPS)？" "n"; then
      CFG_MASQ_TCP=1
    fi
  fi

  CFG_OBFS=0
  CFG_OBFS_PASS=""
  echo
  warn "启用混淆后，客户端须使用相同的混淆密码。"
  if prompt_yn "是否启用混淆？" "n"; then
    CFG_OBFS=1
    CFG_OBFS_PASS="$(prompt "混淆密码" "$(rand_password)")"
  fi

  echo
  title "带宽与拥塞行为"
  CFG_BW_UP="$(prompt "服务器 up（≈客户端下载）" "$DEFAULT_BW_UP")"
  CFG_BW_DOWN="$(prompt "服务器 down（≈客户端上传）" "$DEFAULT_BW_DOWN")"
  CFG_DISABLE_LOSS_COMP=1
  echo
  if prompt_yn "关闭服务端发送方向丢包补偿？" "y"; then
    CFG_DISABLE_LOSS_COMP=1
  else
    CFG_DISABLE_LOSS_COMP=0
  fi

  echo
  CFG_SNIFF=0
  if prompt_yn "启用协议嗅探 sniff？（域名识别/ACL，默认关闭）" "n"; then
    CFG_SNIFF=1
  else
    CFG_SNIFF=0
  fi
  CFG_SPEEDTEST=0
  if prompt_yn "启用内置速度测试 speedTest？" "n"; then
    CFG_SPEEDTEST=1
  fi

  local detected_ip
  detected_ip="$(detect_public_ip)"
  if [[ "$CFG_CERT_MODE" == "acme" ]]; then
    CFG_SERVER_ADDR="$(prompt "客户端连接地址（域名）" "$CFG_DOMAIN")"
  else
    CFG_SERVER_ADDR="$(prompt "客户端连接地址（IP 或域名）" "${detected_ip:-}")"
    [[ -n "$CFG_SERVER_ADDR" ]] || { err "连接地址不能为空"; exit 1; }
  fi

  echo
  local last_export="" cwd_export
  cwd_export="$(pwd -P 2>/dev/null || pwd)"
  [[ -f "$EXPORT_META" ]] && last_export="$(tr -d '\r\n' <"$EXPORT_META" || true)"
  local export_hint="${last_export:-$cwd_export}"
  CFG_EXPORT_DIR="$(prompt "连接信息保存目录（直接回车=当前目录）" "$export_hint")"
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
  apply_config_service || return 1

  maybe_apply_stability_tune

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
  if ! prompt_yn "确认卸载服务程序？" "n"; then
    info "已取消"
    return
  fi
  local keep_cfg=0
  if prompt_yn "是否保留配置与证书目录 ${CONFIG_DIR}？" "y"; then
    keep_cfg=1
  fi
  local dir
  dir="$(resolve_export_dir)"
  local keep_export=1
  if [[ -n "$dir" && -d "$dir" ]]; then
    if ! prompt_yn "是否保留 ${dir} 中的连接信息文件？" "y"; then
      keep_export=0
    fi
  fi
  if ! run_official_installer --remove; then
    err "官方卸载失败，保留配置与连接信息；请排查后重试"
    return 1
  fi
  if [[ -f "$SYSCTL_DROPIN" ]] || [[ -f "$SYSCTL_BACKUP" ]]; then
    if prompt_yn "是否同时撤销 UDP 缓冲区稳定性调优？" "y"; then
      revert_udp_buffer_tune || true
    else
      info "已保留 sysctl 调优（${SYSCTL_DROPIN}）"
    fi
  fi
  if [[ "$keep_cfg" -eq 0 ]]; then
    rm -rf "$CONFIG_DIR"
    info "已删除 $CONFIG_DIR"
  else
    info "已保留 $CONFIG_DIR"
  fi
  if [[ "$keep_export" -eq 0 && -n "$dir" ]]; then
    rm -f -- "$dir/connection.txt" "$dir/client.yaml" "$dir/share-uri.txt" "$dir/surge-proxy.txt"
    info "已删除 $dir 中的连接信息文件（保留目录和其它文件）"
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
    echo -e "${C_BOLD}========== 配置管理 ==========${C_RESET}"
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
    echo " 11) 系统缓冲调优（UDP）"
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
          err "未检测到 ${HY2_BIN}，请先选 1 安装"
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
  tune        应用 UDP 缓冲区稳定性调优
  tune-status 查看缓冲调优状态
  untune      撤销缓冲调优
  uninstall   卸载
  help        帮助

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
