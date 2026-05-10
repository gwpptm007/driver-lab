#!/usr/bin/env bash
#============================================================
# common.sh — AF_XDP socket rings 实验脚本公共函数库
#
# 功能：
#   - 定义环境变量默认值（接口、队列、模式、时长）
#   - 提供记录目录创建、root 检查、AF_XDP 应用运行等公共函数
#   - 被同目录下所有脚本 source 使用
#
# 主要环境变量：
#   AF_XDP_IFACE       : 实验用的网卡名（默认 ens192）
#   AF_XDP_MANAGEMENT_IFACE : 管理网口，不能用于实验（默认 ens33）
#   AF_XDP_PCI         : 网卡 PCI 地址（用于 DPDK 重新绑定）
#   AF_XDP_DRIVER      : 网卡驱动名（vmxnet3/virtio_net 等）
#   AF_XDP_MODE        : XDP 挂载模式 skb（通用）/ native（驱动原生）
#   AF_XDP_QUEUE       : RX 队列号（默认 0）
#   AF_XDP_DURATION    : 程序运行时间（秒，默认 15）
#   AF_XDP_INTERVAL    : 统计打印间隔（秒，默认 1）
#   AF_XDP_BIND_MODE   : AF_XDP 绑定模式 copy / zero-copy
#============================================================

set -euo pipefail

#------------------------------
# 路径变量
#------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # scripts/ 目录
LAB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"                   # lab-af-xdp-socket-rings/ 根目录
APP_DIR="${LAB_DIR}/app"                                     # 应用源码目录（Makefile 所在）
RECORDS_DIR="${LAB_DIR}/records"                             # 测试记录目录
LAB_NAME="af-xdp-socket-rings"                               # 实验名称（用于记录目录后缀）

#------------------------------
# 环境变量默认值
#------------------------------
: "${AF_XDP_IFACE:=ens192}"                # 实验网卡
: "${AF_XDP_MANAGEMENT_IFACE:=ens33}"     # 管理网口（不得用于 AF_XDP 实验，防止 SSH 断开）
: "${AF_XDP_PCI:=0000:0b:00.0}"           # 网卡 PCI 地址（DPDK 绑定用）
: "${AF_XDP_DRIVER:=vmxnet3}"             # 网卡驱动名
: "${AF_XDP_MODE:=skb}"                   # XDP 模式：skb（通用兼容）/ native（驱动原生）
: "${AF_XDP_QUEUE:=0}"                    # 使用的 RX 队列号
: "${AF_XDP_DURATION:=15}"               # 程序运行时间（秒）
: "${AF_XDP_INTERVAL:=1}"                 # 统计打印间隔（秒）
: "${AF_XDP_BIND_MODE:=copy}"            # AF_XDP 绑定模式：copy（拷贝）/ zero-copy（零拷贝）

#------------------------------
# make_record_dir — 创建带时间戳的记录目录
#
# 创建 records/YYYYMMDD-HHMMSS-af-xdp-socket-rings/ 目录并返回路径。
# 每一次完整实验运行对应一个记录目录，内部包含所有日志和结果文件。
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
#
# 优先返回已有的最新记录目录；如果还没有记录目录则新建一个。
# 脚本在非首次运行时（如 01_build_app.sh）使用此函数追加到同一轮记录。
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
#
# AF_XDP 操作（XDP program 挂载、网卡 up/down、PCI rebind）均需 root。
# 如果不是 root，则报错退出。
#------------------------------
require_root() {
    if [[ "${EUID}" -ne 0 ]]; then
        echo "ERROR: this script requires root. Please run with sudo." >&2
        exit 1
    fi
}

#------------------------------
# refuse_management_iface — 禁止在管理网口上操作
#
# 参数：
#   $1 — 操作名称（如 "prepare AF_XDP kernel netdev"）
#
# 实验期间如果不小心在管理网口（ens33）上挂载 XDP，会导致 SSH 断开。
# 此函数检测 AF_XDP_IFACE 是否与管理网口相同，相同则拒绝执行。
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
#
# 将所有关键环境变量输出到 stdout，供日志文件记录实验环境。
# 输出格式为纯文本，方便 human 和 script 解析。
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
AF_XDP_MODE=${AF_XDP_MODE}
AF_XDP_QUEUE=${AF_XDP_QUEUE}
AF_XDP_DURATION=${AF_XDP_DURATION}
AF_XDP_INTERVAL=${AF_XDP_INTERVAL}
AF_XDP_BIND_MODE=${AF_XDP_BIND_MODE}
EOF2
}

#------------------------------
# mode_arg — 将 AF_XDP_MODE 环境变量转为命令行参数
#
# 返回值：
#   skb 或 native，供 --mode 参数使用
#------------------------------
mode_arg() {
    case "${AF_XDP_MODE}" in
        skb|native) echo "${AF_XDP_MODE}" ;;
        *) echo "ERROR: invalid AF_XDP_MODE=${AF_XDP_MODE}, use skb/native" >&2; exit 2 ;;
    esac
}

#------------------------------
# bind_mode_arg — 将 AF_XDP_BIND_MODE 环境变量转为命令行参数
#
# 返回值：
#   --copy 或 --zero-copy，供 af_xdp_rings 程序使用
#------------------------------
bind_mode_arg() {
    case "${AF_XDP_BIND_MODE}" in
        copy) echo "--copy" ;;
        zero-copy|zerocopy|zc) echo "--zero-copy" ;;
        *) echo "ERROR: invalid AF_XDP_BIND_MODE=${AF_XDP_BIND_MODE}, use copy/zero-copy" >&2; exit 2 ;;
    esac
}

#------------------------------
# run_af_xdp_app — 运行 AF_XDP 应用程序
#
# 参数：
#   $1 — 日志输出文件路径
#
# 流程：
#   1. 进入 APP_DIR（app/ 目录）
#   2. 根据当前环境变量拼装完整命令行
#   3. 执行 sudo af_xdp_rings（UMEM 创建、socket 绑定、XDP attach、poll 收包）
#   4. 所有输出同时 tee 到终端和日志文件
#
# 注意：
#   程序需要 root 权限（XDP attach、hugepage/MLOCK 限制）。
#   此函数不检查 root，由调用方脚本确保 require_root。
#------------------------------
run_af_xdp_app() {
    local out="$1"
    mkdir -p "$(dirname "${out}")"
    (
        cd "${APP_DIR}"
        local bind_arg
        bind_arg="$(bind_mode_arg)"
        echo "COMMAND: sudo ./build/af_xdp_rings --ifname ${AF_XDP_IFACE} --queue ${AF_XDP_QUEUE} --mode $(mode_arg) ${bind_arg} --duration ${AF_XDP_DURATION} --interval ${AF_XDP_INTERVAL} --obj ./build/af_xdp_kern.bpf.o"
        ./build/af_xdp_rings \
            --ifname "${AF_XDP_IFACE}" \
            --queue "${AF_XDP_QUEUE}" \
            --mode "$(mode_arg)" \
            ${bind_arg} \
            --duration "${AF_XDP_DURATION}" \
            --interval "${AF_XDP_INTERVAL}" \
            --obj ./build/af_xdp_kern.bpf.o
    ) 2>&1 | tee "${out}"
}