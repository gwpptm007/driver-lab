#!/usr/bin/env bash
#===============================================================================
# common.sh - 共享变量和函数库
# 作用：所有脚本通过 source 引入，定义环境变量和公共函数
# 主要变量：DPDK_IF, DPDK_PCI, HUGEPAGES, fastpath-lite 路径等
# 主要函数：log_env, need_cmd, latest_record_dir
#===============================================================================
set -euo pipefail

#==============================================================================
# 路径设置（根据脚本所在目录自动推导）
# SCRIPT_DIR     → scripts/
# PROJECT_ROOT   → project-fastpath-traffic-test/
# TRACK_DPDK_ROOT → track-dpdk/
# FASTPATH_PROJECT_DIR → ../project-user-space-fastpath/
#==============================================================================
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
TRACK_DPDK_ROOT=$(cd "${PROJECT_ROOT}/.." && pwd)
FASTPATH_PROJECT_DIR="${TRACK_DPDK_ROOT}/project-user-space-fastpath"
FASTPATH_APP_DIR="${FASTPATH_PROJECT_DIR}/app"
FASTPATH_BIN="${FASTPATH_APP_DIR}/build/fastpath-lite"

#==============================================================================
# 环境变量默认值（可通过环境变量覆盖）
# DPDK 参数：DPDK_IF（网卡名）、DPDK_PCI（地址）、DPDK_DRIVER（uio_pci_generic）
# fastpath-lite 参数：运行时长、统计周期、burst size、lcore、UDP/REWRITE 开关等
# 发包参数：目的 IP、端口、MAC、发包数量（用于外部发包参考）
#==============================================================================
: "${DPDK_IF:=ens192}"
: "${DPDK_PCI:=0000:0b:00.0}"
: "${DPDK_DRIVER:=uio_pci_generic}"
: "${MGMT_IF:=ens33}"
: "${MGMT_PCI:=0000:02:01.0}"
: "${HUGEPAGES:=1024}"
: "${FASTPATH_RUN_SECONDS:=30}"
: "${FASTPATH_STATS_PERIOD:=2}"
: "${FASTPATH_BURST_SIZE:=32}"
: "${FASTPATH_LCORES:=0-1}"
: "${FASTPATH_MEMORY_CHANNELS:=4}"
: "${FASTPATH_FILE_PREFIX:=fastpath_traffic_test}"
: "${FASTPATH_PROMISC:=1}"
: "${FASTPATH_UDP_ONLY:=1}"
: "${FASTPATH_SWAP_MAC:=1}"
: "${FASTPATH_REWRITE_ENABLE:=0}"
: "${FASTPATH_EXTRA_APP_ARGS:=}"
: "${SENDER_IF:=}"
: "${SENDER_DST_MAC:=}"
: "${SENDER_DST_IP:=192.168.100.1}"
: "${SENDER_DST_PORT:=9000}"
: "${SENDER_COUNT:=1000}"

#==============================================================================
# 记录目录初始化
# RECORD_TAG   → 时间戳标签，如 20260507_222832-fastpath-traffic-test
# RECORD_DIR   → records/<RECORD_TAG>/，所有输出文件放此处
#==============================================================================
RECORD_TAG=${RECORD_TAG:-$(date +%Y%m%d_%H%M%S)-fastpath-traffic-test}
RECORD_DIR=${RECORD_DIR:-${PROJECT_ROOT}/records/${RECORD_TAG}}
mkdir -p "${RECORD_DIR}"

#------------------------------------------------------------------------------
# log_env - 输出当前所有环境变量到 stdout
# 用途：每个脚本开头调用，记录运行时配置，便于复现和 debug
#------------------------------------------------------------------------------
log_env() {
  cat <<EOT
PROJECT_ROOT=${PROJECT_ROOT}
FASTPATH_PROJECT_DIR=${FASTPATH_PROJECT_DIR}
FASTPATH_BIN=${FASTPATH_BIN}
DPDK_IF=${DPDK_IF}
DPDK_PCI=${DPDK_PCI}
DPDK_DRIVER=${DPDK_DRIVER}
MGMT_IF=${MGMT_IF}
MGMT_PCI=${MGMT_PCI}
HUGEPAGES=${HUGEPAGES}
FASTPATH_RUN_SECONDS=${FASTPATH_RUN_SECONDS}
FASTPATH_STATS_PERIOD=${FASTPATH_STATS_PERIOD}
FASTPATH_BURST_SIZE=${FASTPATH_BURST_SIZE}
FASTPATH_LCORES=${FASTPATH_LCORES}
FASTPATH_MEMORY_CHANNELS=${FASTPATH_MEMORY_CHANNELS}
FASTPATH_FILE_PREFIX=${FASTPATH_FILE_PREFIX}
FASTPATH_PROMISC=${FASTPATH_PROMISC}
FASTPATH_UDP_ONLY=${FASTPATH_UDP_ONLY}
FASTPATH_SWAP_MAC=${FASTPATH_SWAP_MAC}
FASTPATH_REWRITE_ENABLE=${FASTPATH_REWRITE_ENABLE}
FASTPATH_EXTRA_APP_ARGS=${FASTPATH_EXTRA_APP_ARGS}
RECORD_DIR=${RECORD_DIR}
EOT
}

#------------------------------------------------------------------------------
# need_cmd - 检查命令是否存在，不存在则报错退出
# 参数：$1 命令名
# 用途：确保关键工具（meson, ninja, python3 等）已安装
#------------------------------------------------------------------------------
need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "[MISS] command not found: $1" >&2
    return 1
  }
}

#------------------------------------------------------------------------------
# latest_record_dir - 返回 records/ 下最新的目录路径
# 用途：07_compare_stats.sh 等脚本不传参数时自动找到最新记录
#------------------------------------------------------------------------------
latest_record_dir() {
  find "${PROJECT_ROOT}/records" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sort | tail -1
}
