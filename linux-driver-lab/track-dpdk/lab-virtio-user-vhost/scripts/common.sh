#!/usr/bin/env bash
# 公共函数库 - lab-virtio-user-vhost
# 功能: DPDK virtio-user frontend 与 vhost-user backend 配对测试辅助函数
#
# ========== 核心架构 ==========
# 本实验同时运行两个 DPDK testpmd 进程：
#
# 1. backend（后端）: net_vhost0
#    - DPDK vhost-user PMD，作为 virtio 设备的数据面 backend
#    - 在 server 模式下创建 UDS socket（iface=${VHOST_SOCKET}）
#    - 负责接收来自 frontend 的数据包
#    - 转发模式: rxonly（只接收，不发送）
#
# 2. frontend（前端）: net_virtio_user0
#    - DPDK virtio-user PMD，模拟 virtio 网络设备的前端驱动
#    - 作为 client 连接到 backend 创建的 UDS socket
#    - 负责发送数据包给 backend
#    - 转发模式: txonly（只发送，不接收）
#
# 通信链路:
#   frontend (txonly) ──UDS socket──► backend (rxonly)
#                         /tmp/vhost.sock
#
# virtqueue 机制:
#   - virtqueue 是 frontend 和 backend 之间的共享内存区域
#   - frontend 将要发送的数据包描述符写入 available ring
#   - backend 从 available ring 取走数据，写入 used ring 通知完成
#   - 关键：通过 UDS 传递文件描述符（FD），实现跨进程共享内存
#
# ==============================================

set -euo pipefail

LAB_NAME="virtio-user-vhost"
LAB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 管理网卡配置（本实验不使用）
: "${MGMT_IF:=ens33}"
: "${MGMT_PCI:=0000:02:01.0}"
# 大页配置：2MB × 1024 = 2GB
: "${HUGEPAGES:=1024}"
: "${HUGEPAGE_MOUNT:=/mnt/huge}"
# UDS socket 路径
: "${VHOST_SOCKET:=/tmp/dpdk-vhost-user0}"
# vhost/virtio 队列数量（1 = 1个 Rx + 1个 Tx virtqueue）
: "${VHOST_QUEUES:=1}"
# vhost-user 模式：0=server（DPDK 创建 socket），1=client（DPDK 连接）
: "${VHOST_CLIENT_MODE:=0}"
# virtio-user 模式：0=client（连接 socket），1=server（监听 socket）
: "${VIRTIO_SERVER_MODE:=0}"
# backend testpmd 运行时长（秒）
: "${BACKEND_RUNTIME:=28}"
# frontend testpmd 运行时长（秒）
: "${FRONTEND_RUNTIME:=20}"
# 等待 frontend/backend 协商完成的预热时间（秒）
: "${PAIR_WARMUP_SECONDS:=6}"
# backend 使用的 CPU 核
: "${BACKEND_CORES:=0-1}"
# frontend 使用的 CPU 核
: "${FRONTEND_CORES:=2-3}"
# 内存通道数
: "${TESTPMD_MEM_CHANNELS:=4}"
# backend 文件前缀（用于 hugepage 文件隔离）
: "${BACKEND_FILE_PREFIX:=vhost_backend}"
# frontend 文件前缀
: "${FRONTEND_FILE_PREFIX:=virtio_frontend}"
# 统计信息打印周期（秒）
: "${TESTPMD_STATS_PERIOD:=2}"
# backend 转发模式：rxonly（只接收）
: "${BACKEND_FORWARD_MODE:=rxonly}"
# frontend 转发模式：txonly（只发送）
: "${FRONTEND_FORWARD_MODE:=txonly}"
# 端口拓扑：chained（链式）
: "${BACKEND_PORT_TOPOLOGY:=chained}"
: "${FRONTEND_PORT_TOPOLOGY:=chained}"
# 不扫描/绑定物理 PCI 设备
: "${NO_PCI:=1}"

# 可选扩展参数（兼容不同 DPDK 版本）
: "${BACKEND_VDEV_EXTRA:=}"
: "${FRONTEND_VDEV_EXTRA:=}"
: "${BACKEND_APP_EXTRA:=}"
: "${FRONTEND_APP_EXTRA:=}"

# 生成时间戳，格式：YYYYMMDD_HHMMSS
timestamp() {
    date +"%Y%m%d_%H%M%S"
}

# 创建新的记录目录
new_record_dir() {
    local ts
    ts="$(timestamp)"
    echo "${LAB_ROOT}/records/${ts}-${LAB_NAME}"
}

# 查找最新创建的记录目录
latest_record_dir() {
    local latest
    latest="$(find "${LAB_ROOT}/records" -maxdepth 1 -type d -name "*-${LAB_NAME}" 2>/dev/null | sort | tail -n 1 || true)"
    if [[ -n "${latest}" ]]; then
        echo "${latest}"
    else
        new_record_dir
    fi
}

# 确保记录目录存在
ensure_record_dir() {
    if [[ -n "${RECORD_DIR:-}" ]]; then
        mkdir -p "${RECORD_DIR}"
        echo "${RECORD_DIR}"
        return
    fi
    local dir
    dir="$(latest_record_dir)"
    mkdir -p "${dir}"
    echo "${dir}"
}

# 执行命令并捕获输出到指定文件
run_capture() {
    local out="$1"
    shift
    {
        echo "\$ $*"
        "$@"
    } >> "${out}" 2>&1 || {
        local rc=$?
        echo "[WARN] command failed rc=${rc}: $*" >> "${out}"
        return 0
    }
}

# 查找 dpdk-testpmd 工具路径
find_testpmd() {
    local candidates=(
        "${TESTPMD_BIN:-}"
        "dpdk-testpmd"
        "testpmd"
        "/usr/bin/dpdk-testpmd"
        "/usr/local/bin/dpdk-testpmd"
        "/opt/dpdk/build/app/dpdk-testpmd"
        "/opt/dpdk/build/app/testpmd"
    )
    local c
    for c in "${candidates[@]}"; do
        [[ -z "${c}" ]] && continue
        if [[ "${c}" == */* && -x "${c}" ]]; then
            echo "${c}"
            return 0
        fi
        if command -v "${c}" >/dev/null 2>&1; then
            command -v "${c}"
            return 0
        fi
    done
    return 1
}

# 要求以 root 权限运行
require_root_for_write() {
    if [[ "${EUID}" -ne 0 ]]; then
        echo "ERROR: this action may modify hugepages/runtime sockets; please run with sudo." >&2
        exit 1
    fi
}

# 记录执行的命令到 COMMANDS.md
append_command_log() {
    local record_dir="$1"
    shift
    {
        echo
        echo "## $(date '+%F %T')"
        echo '```bash'
        printf '%q ' "$@"
        echo
        echo '```'
    } >> "${record_dir}/COMMANDS.md"
}

# 初始化记录文件
init_record_files() {
    local record_dir="$1"
    mkdir -p "${record_dir}"
    [[ -f "${record_dir}/COMMANDS.md" ]] || cat > "${record_dir}/COMMANDS.md" <<'EOC'
# COMMANDS

EOC
    [[ -f "${record_dir}/SUMMARY.md" ]] || cat > "${record_dir}/SUMMARY.md" <<'EOF_SUMMARY'
# SUMMARY

## Lab

lab-virtio-user-vhost

## 测试机环境

- Guest: Ubuntu 22.04.5 Desktop
- Kernel: Linux 6.8.0-110-generic
- 管理网卡: ${MGMT_IF}
- vhost-user socket: ${VHOST_SOCKET}

## 目标

- 复用 hugepage/testpmd 环境
- 使用 backend testpmd 启动 net_vhost
- 使用 frontend testpmd 启动 net_virtio_user
- 通过 UNIX socket 对接 frontend/backend
- 输出两边 port info/stats
- 不操作物理网卡 bind/unbind

## 结果

- 待填写

## 问题

- 待填写

## 下一步

- 待填写
EOF_SUMMARY
    [[ -f "${record_dir}/RESULT.md" ]] || cat > "${record_dir}/RESULT.md" <<'EOF_RESULT'
# RESULT

## Pass / Fail

待填写

## Evidence

待填写

## Review

待填写
EOF_RESULT
}

# 打印实验室环境变量
print_lab_env() {
    cat <<EOF_ENV
LAB_ROOT=${LAB_ROOT}
MGMT_IF=${MGMT_IF}
MGMT_PCI=${MGMT_PCI}
HUGEPAGES=${HUGEPAGES}
HUGEPAGE_MOUNT=${HUGEPAGE_MOUNT}
VHOST_SOCKET=${VHOST_SOCKET}
VHOST_QUEUES=${VHOST_QUEUES}
VHOST_CLIENT_MODE=${VHOST_CLIENT_MODE}
VIRTIO_SERVER_MODE=${VIRTIO_SERVER_MODE}
BACKEND_RUNTIME=${BACKEND_RUNTIME}
FRONTEND_RUNTIME=${FRONTEND_RUNTIME}
PAIR_WARMUP_SECONDS=${PAIR_WARMUP_SECONDS}
BACKEND_CORES=${BACKEND_CORES}
FRONTEND_CORES=${FRONTEND_CORES}
TESTPMD_MEM_CHANNELS=${TESTPMD_MEM_CHANNELS}
BACKEND_FILE_PREFIX=${BACKEND_FILE_PREFIX}
FRONTEND_FILE_PREFIX=${FRONTEND_FILE_PREFIX}
TESTPMD_STATS_PERIOD=${TESTPMD_STATS_PERIOD}
BACKEND_FORWARD_MODE=${BACKEND_FORWARD_MODE}
FRONTEND_FORWARD_MODE=${FRONTEND_FORWARD_MODE}
BACKEND_PORT_TOPOLOGY=${BACKEND_PORT_TOPOLOGY}
FRONTEND_PORT_TOPOLOGY=${FRONTEND_PORT_TOPOLOGY}
NO_PCI=${NO_PCI}
BACKEND_VDEV_EXTRA=${BACKEND_VDEV_EXTRA}
FRONTEND_VDEV_EXTRA=${FRONTEND_VDEV_EXTRA}
BACKEND_APP_EXTRA=${BACKEND_APP_EXTRA}
FRONTEND_APP_EXTRA=${FRONTEND_APP_EXTRA}
EOF_ENV
}

# 生成 backend (vhost-user) 虚拟设备参数
# net_vhost0: DPDK vhost-user PMD，作为 virtio backend
# iface: UDS socket 路径，backend 在此路径创建 socket 并监听
# queues: vring 数量（每个队列包含 available ring 和 used ring）
# client: 0=server 模式（DPDK 创建 socket），1=client 模式（DPDK 连接）
backend_vdev_arg() {
    local arg="net_vhost0,iface=${VHOST_SOCKET},queues=${VHOST_QUEUES}"
    if [[ -n "${VHOST_CLIENT_MODE}" ]]; then
        arg="${arg},client=${VHOST_CLIENT_MODE}"
    fi
    if [[ -n "${BACKEND_VDEV_EXTRA}" ]]; then
        arg="${arg},${BACKEND_VDEV_EXTRA}"
    fi
    echo "${arg}"
}

# 生成 frontend (virtio-user) 虚拟设备参数
# net_virtio_user0: DPDK virtio-user PMD，模拟 virtio 前端驱动
# path: 连接的 UDS socket 路径（必须与 backend 的 iface 相同）
# queues: virtqueue 数量（必须与 backend 相同）
# server: 0=client 模式（frontend 主动连接），1=server 模式（frontend 监听）
frontend_vdev_arg() {
    local arg="net_virtio_user0,path=${VHOST_SOCKET},queues=${VHOST_QUEUES}"
    if [[ -n "${VIRTIO_SERVER_MODE}" ]]; then
        arg="${arg},server=${VIRTIO_SERVER_MODE}"
    fi
    if [[ -n "${FRONTEND_VDEV_EXTRA}" ]]; then
        arg="${arg},${FRONTEND_VDEV_EXTRA}"
    fi
    echo "${arg}"
}

# 检查 socket 路径是否在安全目录
safe_socket_path_check() {
    case "${VHOST_SOCKET}" in
        /tmp/*|/var/run/*|/run/*)
            return 0
            ;;
        *)
            echo "ERROR: VHOST_SOCKET=${VHOST_SOCKET} is outside /tmp, /run or /var/run; refuse cleanup/start." >&2
            exit 2
            ;;
    esac
}

# 检查进程是否存活
pid_alive() {
    local pid="$1"
    [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null
}
