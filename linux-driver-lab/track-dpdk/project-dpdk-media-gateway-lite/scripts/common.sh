#!/usr/bin/env bash
#===============================================================================
# common.sh - 共享变量和函数库
# 作用：所有脚本通过 source 引入，定义环境变量和公共函数
# 主要变量：DPDK_IF, DPDK_PCI, MEDIA_RUN_SECONDS, 规则配置等
# 主要函数：log_env, find_dpdk_devbind, latest_record_dir, base_app_args, require_media_bin
#===============================================================================
set -euo pipefail

#==============================================================================
# 路径设置（根据脚本所在目录自动推导）
#==============================================================================
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
TRACK_DPDK_ROOT=$(cd "${PROJECT_ROOT}/.." && pwd)
APP_DIR="${PROJECT_ROOT}/app"
MEDIA_BIN="${APP_DIR}/build/media-gateway-lite"
UPSTREAM_FASTPATH_DIR="${TRACK_DPDK_ROOT}/project-user-space-fastpath"

#==============================================================================
# DPDK 环境变量默认值
# DPDK_IF/PDEB PCI：指定 DPDK 使用的网卡
# MGMT_IF/PCI：管理口（不被 DPDK 操作）
# HUGEPAGES： hugepage 大小（MB）
#==============================================================================
: "${DPDK_IF:=ens192}"
: "${DPDK_PCI:=0000:0b:00.0}"
: "${DPDK_DRIVER:=uio_pci_generic}"
: "${MGMT_IF:=ens33}"
: "${MGMT_PCI:=0000:02:01.0}"
: "${HUGEPAGES:=1024}"

#==============================================================================
# media-gateway-lite 运行参数默认值
#==============================================================================
: "${MEDIA_RUN_SECONDS:=20}"       # 运行时间（秒），0=无限
: "${MEDIA_STATS_PERIOD:=2}"       # 统计打印周期（秒）
: "${MEDIA_BURST_SIZE:=32}"        # RX/TX burst 大小
: "${MEDIA_LCORES:=0-1}"          # 使用的 lcore
: "${MEDIA_MEMORY_CHANNELS:=4}"    # 内存通道数
: "${MEDIA_FILE_PREFIX:=media_gateway_lite}"  # DPDK file prefix（用于多实例隔离）
: "${MEDIA_PROMISC:=1}"            # 混杂模式开关
: "${MEDIA_UDP_ONLY:=1}"           # UDP-only 策略
: "${MEDIA_SWAP_MAC:=1}"          # 无 MAC rewrite 时交换 MAC
: "${MEDIA_STRICT_RULES:=1}"      # 严格模式（无匹配则丢弃）
: "${MEDIA_EXTRA_APP_ARGS:=}"     # 额外 APP 参数

#==============================================================================
# 规则配置默认值（用于 rewrite 测试）
#==============================================================================
: "${RULE0_DIR:=0:1}"                           # 规则 0 方向：port0 → port1
: "${RULE1_DIR:=1:0}"                           # 规则 1 方向：port1 → port0
: "${RULE0_NAME:=access_to_core}"
: "${RULE1_NAME:=core_to_access}"
: "${RULE0_DST_PORT:=9000}"                    # 匹配目的端口
: "${RULE1_DST_PORT:=10000}"
: "${RULE0_REWRITE_DST_MAC:=52:54:00:00:00:02}"  # rewrite 目标 MAC
: "${RULE1_REWRITE_DST_MAC:=52:54:00:00:00:01}"
: "${RULE0_REWRITE_DST_IP:=10.10.20.20}"          # rewrite 目标 IP
: "${RULE1_REWRITE_DST_IP:=10.10.10.10}"
: "${RULE0_REWRITE_DST_PORT:=10000}"              # rewrite 目标端口
: "${RULE1_REWRITE_DST_PORT:=9000}"

#==============================================================================
# 记录目录初始化
# RECORD_DIR：records/<时间戳>-media-gateway-lite/，所有输出文件放此处
#==============================================================================
if [[ -z "${RECORD_DIR:-}" ]]; then
  RECORD_DIR="${PROJECT_ROOT}/records/$(date +%Y%m%d-%H%M%S)-media-gateway-lite"
fi
mkdir -p "${RECORD_DIR}"

#------------------------------------------------------------------------------
# log_env - 输出当前所有环境变量到 stdout
# 用途：每个脚本开头调用，记录运行时配置，便于复现和 debug
#------------------------------------------------------------------------------
log_env() {
  cat <<EOF
PROJECT_ROOT=${PROJECT_ROOT}
APP_DIR=${APP_DIR}
MEDIA_BIN=${MEDIA_BIN}
RECORD_DIR=${RECORD_DIR}
DPDK_IF=${DPDK_IF}
DPDK_PCI=${DPDK_PCI}
DPDK_DRIVER=${DPDK_DRIVER}
MGMT_IF=${MGMT_IF}
MGMT_PCI=${MGMT_PCI}
MEDIA_RUN_SECONDS=${MEDIA_RUN_SECONDS}
MEDIA_LCORES=${MEDIA_LCORES}
MEDIA_FILE_PREFIX=${MEDIA_FILE_PREFIX}
EOF
}

#------------------------------------------------------------------------------
# find_dpdk_devbind - 查找 dpdk-devbind.py 工具路径
# 用途：在常见路径中查找 DPDK 提供的网卡绑定工具
#------------------------------------------------------------------------------
find_dpdk_devbind() {
  for p in /usr/share/dpdk/usertools/dpdk-devbind.py /usr/local/share/dpdk/usertools/dpdk-devbind.py dpdk-devbind.py; do
    if [[ -x "$p" ]] || command -v "$p" >/dev/null 2>&1; then
      echo "$p"
      return 0
    fi
  done
  return 1
}

#------------------------------------------------------------------------------
# latest_record_dir - 返回 records/ 下最新的目录路径
# 用途：其他脚本不传参数时自动找到最新记录
#------------------------------------------------------------------------------
latest_record_dir() {
  find "${PROJECT_ROOT}/records" -maxdepth 1 -type d -name '*-media-gateway-lite' | sort | tail -1
}

#------------------------------------------------------------------------------
# base_app_args - 生成基础 APP 参数（null 分隔）
# 用途：统一的策略参数（run_seconds, udp_only, swap_mac 等）
#------------------------------------------------------------------------------
base_app_args() {
  printf '%s\0' \
    --run-seconds "${MEDIA_RUN_SECONDS}" \
    --stats-period "${MEDIA_STATS_PERIOD}" \
    --burst-size "${MEDIA_BURST_SIZE}" \
    --promisc "${MEDIA_PROMISC}" \
    --udp-only "${MEDIA_UDP_ONLY}" \
    --swap-mac "${MEDIA_SWAP_MAC}" \
    --strict-rules "${MEDIA_STRICT_RULES}"
}

#------------------------------------------------------------------------------
# require_media_bin - 检查 media-gateway-lite binary 是否存在
# 用途：运行脚本前确保已编译
#------------------------------------------------------------------------------
require_media_bin() {
  if [[ ! -x "${MEDIA_BIN}" ]]; then
    echo "[ERR] media binary not found: ${MEDIA_BIN}" >&2
    echo "Run: ./scripts/01_build_app.sh" >&2
    exit 1
  fi
}
