#!/usr/bin/env bash
#============================================================
# common.sh — af-xdp-zero-copy-vs-copy 实验脚本公共函数库
#
# 功能：
#   - 定义环境变量默认值（接口、队列、模式、时长）
#   - 提供记录目录创建、root 检查、probe 运行等公共函数
#   - 被同目录下所有脚本 source 使用
#
# 主要环境变量：
#   AF_XDP_IFACE          : 实验用的网卡名（默认 ens192）
#   AF_XDP_MANAGEMENT_IFACE : 管理网口，不能用于实验（默认 ens33）
#   AF_XDP_PCI            : 网卡 PCI 地址（用于 DPDK 重新绑定）
#   AF_XDP_DRIVER         : 网卡驱动名（vmxnet3/virtio_net 等）
#   AF_XDP_QUEUE          : RX 队列号（默认 0）
#   AF_XDP_DURATION       : 程序运行时间（秒，默认 8）
#   AF_XDP_INTERVAL       : 统计打印间隔（秒，默认 1）
#============================================================

set -euo pipefail

#------------------------------
# 路径变量
#------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # scripts/ 目录
LAB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"                    # lab-af-xdp-zero-copy-vs-copy/ 根目录
APP_DIR="${LAB_DIR}/app"                                     # 应用源码目录（Makefile 所在）
RECORDS_DIR="${LAB_DIR}/records"                             # 测试记录目录
LAB_NAME="af-xdp-zero-copy-vs-copy"                          # 实验名称（用于记录目录后缀）

#------------------------------
# 环境变量默认值
#------------------------------
: "${AF_XDP_IFACE:=ens192}"
: "${AF_XDP_MANAGEMENT_IFACE:=ens33}"
: "${AF_XDP_PCI:=0000:0b:00.0}"
: "${AF_XDP_DRIVER:=vmxnet3}"
: "${AF_XDP_QUEUE:=0}"
: "${AF_XDP_DURATION:=8}"           # 比 rings lab 短，用于快速探测
: "${AF_XDP_INTERVAL:=1}"

#------------------------------
# make_record_dir — 创建带时间戳的记录目录
#------------------------------
make_record_dir() {
    local ts
    ts="$(date +%Y%m%d-%H%M%S)"
    local dir="${RECORDS_DIR}/${ts}-${LAB_NAME}"
    mkdir -p "${dir}"
    echo "${dir}"
}

#------------------------------
# latest_record_dir — 返回最新的记录目录
#------------------------------
latest_record_dir() {
    local latest
    latest="$(find "${RECORDS_DIR}" -maxdepth 1 -type d -name "*-${LAB_NAME}" | sort | tail -1 || true)"
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
LAB=${LAB_NAME}
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
EOF2
}

#------------------------------
# run_probe — 运行 af_xdp_mode_probe 并记录返回值
#
# 参数：
#   $1 — mode（skb / native）
#   $2 — bind（copy / zero-copy）
#   $3 — 输出日志文件路径
#   $4 — 返回码文件路径
#
# 流程：
#   进入 APP_DIR，执行 af_xdp_mode_probe，
#   捕获返回值（PROBE_RC），写入返回码文件（0=成功，非0=失败）。
#   所有输出同时 tee 到终端和日志文件。
#
# 注意：
#   使用 set +e 捕获非零返回码（probe 预期会失败，不需要立即退出）。
#------------------------------
run_probe() {
    local mode="$1"
    local bind="$2"
    local out="$3"
    local rcfile="$4"
    mkdir -p "$(dirname "${out}")"
    (
        cd "${APP_DIR}"
        echo "COMMAND: sudo ./build/af_xdp_mode_probe --ifname ${AF_XDP_IFACE} --queue ${AF_XDP_QUEUE} --mode ${mode} --${bind} --duration ${AF_XDP_DURATION} --interval ${AF_XDP_INTERVAL} --obj ./build/af_xdp_kern.bpf.o"
        set +e  # 允许非零返回码继续执行（probe 失败是预期的）
        ./build/af_xdp_mode_probe \
            --ifname "${AF_XDP_IFACE}" \
            --queue "${AF_XDP_QUEUE}" \
            --mode "${mode}" \
            --"${bind}" \
            --duration "${AF_XDP_DURATION}" \
            --interval "${AF_XDP_INTERVAL}" \
            --obj ./build/af_xdp_kern.bpf.o
        local rc=$?
        echo "PROBE_RC=${rc}"
        echo "${rc}" > "${rcfile}"
        exit 0  # tee 需要成功退出，否则管道断裂
    ) 2>&1 | tee "${out}"
}