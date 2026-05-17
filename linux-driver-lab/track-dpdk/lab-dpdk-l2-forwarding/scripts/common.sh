#!/usr/bin/env bash
# =============================================================================
# common.sh — 所有脚本的公共基础设施
#
# 作用：
#   1. 定义环境变量（PCI 地址、hugepage、运行参数等）
#   2. 提供辅助函数（记录目录管理、安全检查、工具查找等）
#   3. 被其他脚本 source 引入，不单独执行
# =============================================================================
set -euo pipefail

# ── 基本路径 ──────────────────────────────────────────────────────────────────
LAB_NAME="dpdk-l2-forwarding"
# LAB_ROOT: 本 lab 的根目录（scripts/ 的上一级）
LAB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# APP_DIR: C 源码和 meson.build 所在目录
APP_DIR="${LAB_ROOT}/app"
# BUILD_DIR: meson 编译输出目录
BUILD_DIR="${APP_DIR}/build"
# APP_BIN: 编译出来的可执行文件路径
APP_BIN="${BUILD_DIR}/l2fwd-lite"

# ── DPDK 网卡配置 ────────────────────────────────────────────────────────────
# DPDK_IF: DPDK 专用网卡接口名（VMware 虚拟机上的第二块网卡）
: "${DPDK_IF:=ens192}"
# DPDK_PCI: DPDK 网卡的 PCI 地址
: "${DPDK_PCI:=0000:0b:00.0}"
# DPDK_DRIVER: 用户态驱动类型，当前测试机用 uio_pci_generic（不是 vfio-pci）
: "${DPDK_DRIVER:=uio_pci_generic}"

# ── 管理网卡配置（必须保护，绝不能绑定到 DPDK） ──────────────────────────────
: "${MGMT_IF:=ens33}"
: "${MGMT_PCI:=0000:02:01.0}"

# ── Hugepage 配置 ────────────────────────────────────────────────────────────
# DPDK 需要大页内存来避免 TLB miss，这里默认分配 1024 个 2MB hugepage = 2GB
: "${HUGEPAGES:=1024}"
: "${HUGEPAGE_MOUNT:=/mnt/huge}"

# ── l2fwd-lite 运行参数 ─────────────────────────────────────────────────────
: "${L2FWD_RUN_SECONDS:=15}"        # 运行时长（秒），0 表示无限
: "${L2FWD_STATS_PERIOD:=2}"        # 统计打印间隔（秒）
: "${L2FWD_LCORES:=0-1}"            # 使用的 CPU lcore 范围
: "${L2FWD_MEMORY_CHANNELS:=4}"     # 内存通道数（EAL -n 参数）
: "${L2FWD_FILE_PREFIX:=l2fwd_lite}" # DPDK 文件前缀（多实例隔离）
: "${L2FWD_BURST_SIZE:=32}"         # 每次 rx_burst/tx_burst 的包数
: "${L2FWD_PROMISC:=1}"             # 混杂模式：1=开启，0=关闭

# ── 可选：第二块 DPDK 物理网卡（双端口转发用） ────────────────────────────────
: "${DPDK_PCI_1:=}"

# ── 可选：vdev 虚拟网卡名（net_null smoke 测试用） ────────────────────────────
# net_null 是 DPDK 内置的空操作 PMD，收发包直接丢弃，用于纯逻辑验证
: "${L2FWD_VDEV0:=net_null0}"
: "${L2FWD_VDEV1:=net_null1}"

# =============================================================================
# 辅助函数
# =============================================================================

# 生成时间戳字符串，格式：YYYYMMDD_HHMMSS
timestamp() {
    date +"%Y%m%d_%H%M%S"
}

# 创建新的记录目录，路径格式：records/<timestamp>-dpdk-l2-forwarding/
new_record_dir() {
    local ts
    ts="$(timestamp)"
    echo "${LAB_ROOT}/records/${ts}-${LAB_NAME}"
}

# 找到最新的已有记录目录，没有则新建
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
# 如果环境变量 RECORD_DIR 已设置则使用它，否则找最新的
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

# 运行命令并同时将输出追加到日志文件
# 用法：run_capture <输出文件> <命令> <参数...>
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

# 在多个候选路径中查找 dpdk-devbind.py 工具
# dpdk-devbind.py 用于将 PCI 设备在内核驱动和 DPDK 用户态驱动之间切换
find_devbind() {
    local candidates=(
        "${DPDK_DEVBIND:-}"                                    # 用户手动指定
        "dpdk-devbind.py"                                      # PATH 中直接可用
        "dpdk-devbind"                                         # 可能省略了 .py 后缀
        "/usr/share/dpdk/usertools/dpdk-devbind.py"            # Ubuntu 包管理器安装的路径
        "/usr/local/share/dpdk/usertools/dpdk-devbind.py"      # 源码编译安装的路径
        "/opt/dpdk/usertools/dpdk-devbind.py"                  # 自定义安装路径
    )

    local c
    for c in "${candidates[@]}"; do
        [[ -z "${c}" ]] && continue
        # 带路径的候选：检查是否可执行
        if [[ "${c}" == */* && -x "${c}" ]]; then
            echo "${c}"
            return 0
        fi
        # 不带路径的候选：用 command -v 在 PATH 中查找
        if command -v "${c}" >/dev/null 2>&1; then
            command -v "${c}"
            return 0
        fi
    done
    return 1
}

# 要求 root 权限才能继续（修改系统状态的脚本使用）
require_root_for_write() {
    if [[ "${EUID}" -ne 0 ]]; then
        echo "ERROR: this action modifies system state; please run with sudo." >&2
        exit 1
    fi
}

# 将执行的命令记录追加到 COMMANDS.md，方便事后回溯
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

# 初始化记录目录中的模板文件（如果不存在则创建）
# 包含 COMMANDS.md（命令日志）、SUMMARY.md（实验总结）、RESULT.md（结果）
init_record_files() {
    local record_dir="$1"
    mkdir -p "${record_dir}"
    # 命令日志模板
    [[ -f "${record_dir}/COMMANDS.md" ]] || cat > "${record_dir}/COMMANDS.md" <<'EOC'
# COMMANDS

EOC
    # 实验总结模板
    [[ -f "${record_dir}/SUMMARY.md" ]] || cat > "${record_dir}/SUMMARY.md" <<EOF2
# SUMMARY

## Lab

lab-dpdk-l2-forwarding

## 测试机环境

- Guest: Ubuntu 22.04.5 Desktop
- Kernel: Linux 6.8.0-110-generic
- 管理网卡: ${MGMT_IF}
- DPDK 网卡: ${DPDK_IF}
- DPDK PCI: ${DPDK_PCI}
- 默认 DPDK driver: ${DPDK_DRIVER}

## 目标

- 编译 l2fwd-lite
- 通过 EAL/mempool/ethdev/queue 初始化
- 进入 rx_burst/tx_burst 数据面循环
- 单端口 smoke 或双端口 L2 forwarding
- 输出软件 stats 与 rte_eth_stats

## 结果

- 待填写

## 问题

- 待填写

## 下一步

- 待填写
EOF2
    # 实验结果模板
    [[ -f "${record_dir}/RESULT.md" ]] || cat > "${record_dir}/RESULT.md" <<'EOR'
# RESULT

## Pass / Fail

待填写

## Evidence

待填写

## Review

待填写
EOR
}

# 安全检查：确保 DPDK_PCI 不等于管理网卡的 PCI 地址
# 防止误把管理网卡绑定到 DPDK 驱动导致 SSH 断连
guard_not_mgmt_pci() {
    if [[ "${DPDK_PCI}" == "${MGMT_PCI}" ]]; then
        echo "ERROR: DPDK_PCI=${DPDK_PCI} equals management PCI ${MGMT_PCI}; refuse to continue." >&2
        exit 2
    fi
    if [[ -n "${DPDK_PCI_1}" && "${DPDK_PCI_1}" == "${MGMT_PCI}" ]]; then
        echo "ERROR: DPDK_PCI_1=${DPDK_PCI_1} equals management PCI ${MGMT_PCI}; refuse to continue." >&2
        exit 2
    fi
}

# 打印当前所有 lab 环境变量，用于日志记录
print_lab_env() {
    cat <<EOF2
LAB_ROOT=${LAB_ROOT}
APP_DIR=${APP_DIR}
BUILD_DIR=${BUILD_DIR}
APP_BIN=${APP_BIN}
DPDK_IF=${DPDK_IF}
DPDK_PCI=${DPDK_PCI}
DPDK_PCI_1=${DPDK_PCI_1}
DPDK_DRIVER=${DPDK_DRIVER}
MGMT_IF=${MGMT_IF}
MGMT_PCI=${MGMT_PCI}
HUGEPAGES=${HUGEPAGES}
HUGEPAGE_MOUNT=${HUGEPAGE_MOUNT}
L2FWD_RUN_SECONDS=${L2FWD_RUN_SECONDS}
L2FWD_STATS_PERIOD=${L2FWD_STATS_PERIOD}
L2FWD_LCORES=${L2FWD_LCORES}
L2FWD_MEMORY_CHANNELS=${L2FWD_MEMORY_CHANNELS}
L2FWD_FILE_PREFIX=${L2FWD_FILE_PREFIX}
L2FWD_BURST_SIZE=${L2FWD_BURST_SIZE}
L2FWD_PROMISC=${L2FWD_PROMISC}
L2FWD_VDEV0=${L2FWD_VDEV0}
L2FWD_VDEV1=${L2FWD_VDEV1}
EOF2
}
