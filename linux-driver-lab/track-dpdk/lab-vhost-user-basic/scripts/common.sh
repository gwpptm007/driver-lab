#!/usr/bin/env bash
# 公共函数库 - lab-vhost-user-basic
# 功能: DPDK vhost-user backend 测试辅助函数
# 本实验通过 testpmd 启动 vhost-user backend UNIX socket，无需物理网卡绑定

set -euo pipefail

# 实验室名称
LAB_NAME="vhost-user-basic"
# 实验室根目录
LAB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 默认环境变量：管理网卡接口名和 PCI 地址
: "${MGMT_IF:=ens33}"
: "${MGMT_PCI:=0000:02:01.0}"
# 默认环境变量：大页数量和挂载点
: "${HUGEPAGES:=1024}"
: "${HUGEPAGE_MOUNT:=/mnt/huge}"
# vhost-user UNIX socket 路径
: "${VHOST_SOCKET:=/tmp/dpdk-vhost-user0}"
# vhost-user 队列数量
: "${VHOST_QUEUES:=1}"
# vhost-user 是否为 client 模式（0=server, 1=client）
: "${VHOST_CLIENT_MODE:=0}"
# testpmd 运行时长（秒）
: "${TESTPMD_RUNTIME:=18}"
# testpmd 使用的 CPU 核
: "${TESTPMD_CORES:=0-1}"
# 内存通道数
: "${TESTPMD_MEM_CHANNELS:=4}"
# DPDK 文件前缀（用于多实例隔离）
: "${TESTPMD_FILE_PREFIX:=vhost_basic}"
# 统计信息打印周期（秒）
: "${TESTPMD_STATS_PERIOD:=2}"
# 转发模式
: "${TESTPMD_FORWARD_MODE:=io}"

# 本实验不操作物理网卡（使用 --no-pci，不绑定任何 PCI 设备）
: "${NO_PCI:=1}"

# 生成时间戳，格式：YYYYMMDD_HHMMSS
timestamp() {
    date +"%Y%m%d_%H%M%S"
}

# 创建新的记录目录，格式：records/<timestamp>-<lab_name>
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

# 确保记录目录存在，如未指定则创建新目录
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

# 检查命令是否存在
need_cmd() {
    local cmd="$1"
    command -v "${cmd}" >/dev/null 2>&1
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

# 将执行的命令追加到记录目录的 COMMANDS.md
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

# 初始化记录目录中的文件（COMMANDS.md、SUMMARY.md、RESULT.md）
init_record_files() {
    local record_dir="$1"
    mkdir -p "${record_dir}"
    [[ -f "${record_dir}/COMMANDS.md" ]] || cat > "${record_dir}/COMMANDS.md" <<'EOC'
# COMMANDS

EOC
    [[ -f "${record_dir}/SUMMARY.md" ]] || cat > "${record_dir}/SUMMARY.md" <<EOF_SUMMARY
# SUMMARY

## Lab

lab-vhost-user-basic

## 测试机环境

- Guest: Ubuntu 22.04.5 Desktop
- Kernel: Linux 6.8.0-110-generic
- 管理网卡: ${MGMT_IF}
- vhost-user socket: ${VHOST_SOCKET}

## 目标

- 复用 hugepage/testpmd 环境
- 使用 testpmd 启动 DPDK vhost-user backend
- 创建 UNIX domain socket
- 输出 testpmd port info/stats
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

# 打印实验室环境变量配置
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
TESTPMD_RUNTIME=${TESTPMD_RUNTIME}
TESTPMD_CORES=${TESTPMD_CORES}
TESTPMD_MEM_CHANNELS=${TESTPMD_MEM_CHANNELS}
TESTPMD_FILE_PREFIX=${TESTPMD_FILE_PREFIX}
TESTPMD_STATS_PERIOD=${TESTPMD_STATS_PERIOD}
TESTPMD_FORWARD_MODE=${TESTPMD_FORWARD_MODE}
NO_PCI=${NO_PCI}
EOF_ENV
}

# 生成 vhost-user 虚拟设备参数
# 格式: --vdev=net_vhost0,iface=<socket_path>,queues=<n>[,client=<mode>]
# vhost-user 是 DPDK 的 virtio backend 模拟实现，通过 UNIX socket 与 QEMU/virtio 前端通信
vhost_vdev_arg() {
    local arg="net_vhost0,iface=${VHOST_SOCKET},queues=${VHOST_QUEUES}"
    if [[ -n "${VHOST_CLIENT_MODE}" ]]; then
        arg="${arg},client=${VHOST_CLIENT_MODE}"
    fi
    echo "${arg}"
}

# 检查 vhost socket 路径是否安全（仅允许 /tmp、/var/run、/run 下的路径）
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
