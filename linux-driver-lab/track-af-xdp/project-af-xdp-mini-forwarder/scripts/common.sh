#!/usr/bin/env bash
#============================================================
# common.sh — project-af-xdp-mini-forwarder 实验脚本公共函数库
#
# 功能：
#   - 定义环境变量默认值（接口、队列、模式、时长）
#   - 提供记录目录创建、root 检查、转发器运行等公共函数
#   - 被同目录下所有脚本 source 使用
#
# 主要环境变量：
#   AF_XDP_IFACE          : 实验用的网卡名（默认 ens192）
#   AF_XDP_MANAGEMENT_IFACE : 管理网口，不能用于实验（默认 ens33）
#   AF_XDP_PCI            : 网卡 PCI 地址
#   AF_XDP_DRIVER         : 网卡驱动名（vmxnet3）
#   AF_XDP_QUEUE          : RX 队列号（默认 0）
#   AF_XDP_DURATION        : 程序运行时间（秒，默认 10）
#   AF_XDP_INTERVAL        : 统计打印间隔（秒，默认 1）
#   AF_XDP_MODE            : XDP 模式 skb/native（默认 skb）
#   AF_XDP_BIND           : bind 模式 copy/zero-copy（默认 copy）
#============================================================

set -euo pipefail

#------------------------------
# 路径变量
#------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
APP_DIR="${PROJECT_DIR}/app"
RECORDS_DIR="${PROJECT_DIR}/records"
PROJECT_NAME="af-xdp-mini-forwarder"

#------------------------------
# 环境变量默认值
#------------------------------
: "${AF_XDP_IFACE:=ens192}"
: "${AF_XDP_MANAGEMENT_IFACE:=ens33}"
: "${AF_XDP_PCI:=0000:0b:00.0}"
: "${AF_XDP_DRIVER:=vmxnet3}"
: "${AF_XDP_QUEUE:=0}"
: "${AF_XDP_DURATION:=10}"
: "${AF_XDP_INTERVAL:=1}"
: "${AF_XDP_MODE:=skb}"
: "${AF_XDP_BIND:=copy}"

#------------------------------
# make_record_dir — 创建带时间戳的记录目录
#------------------------------
make_record_dir() {
    local ts
    ts="$(date +%Y%m%d-%H%M%S)"
    local dir="${RECORDS_DIR}/${ts}-${PROJECT_NAME}"
    mkdir -p "${dir}"
    echo "${dir}"
}

#------------------------------
# latest_record_dir — 返回最新的记录目录
#------------------------------
latest_record_dir() {
    local latest
    latest="$(find "${RECORDS_DIR}" -maxdepth 1 -type d -name "*-${PROJECT_NAME}" | sort | tail -1 || true)"
    if [[ -z "${latest}" ]]; then
        latest="$(make_record_dir)"
    fi
    echo "${latest}"
}

#------------------------------
# require_root — 确保以 root 身份运行
#------------------------------
require_root() {
    if [[ "${EUID}" -ne 0 ]]; then
        echo "ERROR: this script requires root. Please run with sudo." >&2
        exit 1
    fi
}

#------------------------------
# refuse_management_iface — 禁止在管理网口上操作
#------------------------------
refuse_management_iface() {
    local op="$1"
    if [[ "${AF_XDP_IFACE}" == "${AF_XDP_MANAGEMENT_IFACE}" ]]; then
        echo "ERROR: refusing to ${op} on management iface ${AF_XDP_MANAGEMENT_IFACE}" >&2
        exit 1
    fi
}

#------------------------------
# write_env_header — 输出环境变量头信息
#------------------------------
write_env_header() {
    cat <<EOF2
PROJECT=${PROJECT_NAME}
DATE=$(date -Is)
HOST=$(hostname)
KERNEL=$(uname -r)
AF_XDP_IFACE=${AF_XDP_IFACE}
AF_XDP_MANAGEMENT_IFACE=${AF_XDP_MANAGEMENT_IFACE}
AF_XDP_PCI=${AF_XDP_PCI}
AF_XDP_DRIVER=${AF_XDP_DRIVER}
AF_XDP_QUEUE=${AF_XDP_QUEUE}
AF_XDP_DURATION=${AF_XDP_DURATION}
AF_XDP_INTERVAL=${AF_XDP_INTERVAL}
AF_XDP_MODE=${AF_XDP_MODE}
AF_XDP_BIND=${AF_XDP_BIND}
EOF2
}

#------------------------------
# run_forwarder — 运行转发器并记录日志
#
# 参数：
#   $1 — forward_mode（drop 或 reflect）
#   $2 — 输出日志文件路径
#
# 流程：
#   1. 进入 APP_DIR，执行 sudo ./build/af_xdp_forwarder
#   2. set +e 捕获非零返回码（转发器失败是合法的）
#   3. 所有输出同时 tee 到终端和日志文件
#   4. 记录 FORWARDER_RC 到日志末尾
#------------------------------
run_forwarder() {
    local forward_mode="$1"
    local out="$2"
    mkdir -p "$(dirname "${out}")"
    (
        cd "${APP_DIR}"
        echo "COMMAND: sudo ./build/af_xdp_forwarder --ifname ${AF_XDP_IFACE} --queue ${AF_XDP_QUEUE} --mode ${AF_XDP_MODE} --${AF_XDP_BIND} --forward ${forward_mode} --duration ${AF_XDP_DURATION} --interval ${AF_XDP_INTERVAL} --obj ./build/af_xdp_forwarder_kern.bpf.o"
        set +e  # 允许非零返回码继续执行
        ./build/af_xdp_forwarder \
            --ifname "${AF_XDP_IFACE}" \
            --queue "${AF_XDP_QUEUE}" \
            --mode "${AF_XDP_MODE}" \
            --"${AF_XDP_BIND}" \
            --forward "${forward_mode}" \
            --duration "${AF_XDP_DURATION}" \
            --interval "${AF_XDP_INTERVAL}" \
            --obj ./build/af_xdp_forwarder_kern.bpf.o
        local rc=$?
        echo "FORWARDER_RC=${rc}"
        exit 0  # tee 需要成功退出
    ) 2>&1 | tee "${out}"
}