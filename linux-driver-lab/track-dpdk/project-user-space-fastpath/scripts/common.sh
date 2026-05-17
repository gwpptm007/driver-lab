#!/usr/bin/env bash
# 文件: common.sh
# 功能: project-user-space-fastpath 项目的公共函数库
#       供所有脚本（01-09_*.sh）调用，提供：
#         - 环境变量定义
#         - 记录目录管理
#         - DPDK 工具查找
#         - 安全检查函数
#         - 应用参数生成
# ==============================================

set -euo pipefail  # 严格模式：命令失败退出、变量未定义报错、管道失败传播

# ========== 项目路径定义 ==========
PROJECT_NAME="user-space-fastpath"         # 项目名称（用于记录目录名后缀）
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"  # 项目根目录
APP_DIR="${PROJECT_ROOT}/app"              # 应用源码目录
BUILD_DIR="${APP_DIR}/build"               # 编译产物目录
APP_BIN="${BUILD_DIR}/fastpath-lite"       # 可执行文件路径

# ========== DPDK 网卡配置 ==========
# 格式：: "${VAR:=default}" 表示：如果 VAR 未定义或为空，则使用默认值
: "${DPDK_IF:=ens192}"                     # DPDK 网卡名称（默认 ens192）
: "${DPDK_PCI:=0000:0b:00.0}"             # DPDK 网卡 PCI 地址（需要先绑定到 DPDK 驱动）
: "${DPDK_PCI_1:=}"                       # 第二个 DPDK 网卡 PCI 地址（双端口场景）
: "${DPDK_DRIVER:=uio_pci_generic}"       # DPDK 用户态驱动类型（uio_pci_generic 或 vfio-pci）

# ========== 管理口配置（防误操作）==========
# 管理口是 SSH 连接的网卡，误操作会导致连接断开
: "${MGMT_IF:=ens33}"                     # 管理口网卡名称
: "${MGMT_PCI:=0000:02:01.0}"            # 管理口 PCI 地址

# ========== 大页内存配置 ==========
# DPDK 需要大页内存（2MB/页）来管理 mbuf 和内存池
: "${HUGEPAGES:=1024}"                    # 大页数量：1024 × 2MB = 2GB
: "${HUGEPAGE_MOUNT:=/mnt/huge}"         # hugetlbfs 挂载点

# ========== fastpath-lite 运行参数 ==========
: "${FASTPATH_LCORES:=0-1}"              # EAL 参数：使用的 CPU 核心（-l）
: "${FASTPATH_MEMORY_CHANNELS:=4}"       # EAL 参数：内存通道数（-n）
: "${FASTPATH_FILE_PREFIX:=fastpath_lite}"  # EAL 参数：文件前缀（隔离多实例）
: "${FASTPATH_RUN_SECONDS:=20}"          # 应用参数：运行时间（--run-seconds）
: "${FASTPATH_STATS_PERIOD:=2}"          # 应用参数：统计打印间隔（--stats-period）
: "${FASTPATH_BURST_SIZE:=32}"           # 应用参数：RX/TX burst 大小（--burst-size）
: "${FASTPATH_PROMISC:=1}"               # 应用参数：混杂模式（--promisc）
: "${FASTPATH_UDP_ONLY:=1}"              # 应用参数：UDP-only 过滤（--udp-only）
: "${FASTPATH_SWAP_MAC:=1}"              # 应用参数：MAC 交换（--swap-mac）
: "${FASTPATH_REWRITE_ENABLE:=0}"        # 应用参数：rewrite 功能开关（--rewrite）
: "${FASTPATH_EXTRA_APP_ARGS:=}"         # 应用参数：额外参数（供 06_run_fastpath_rewrite_demo.sh 使用）

# ========== vdev null pair 虚拟设备配置 ==========
# 用于不需要物理网卡的测试场景（05_run_fastpath_vdev_null_pair.sh、06_run_fastpath_rewrite_demo.sh）
: "${FASTPATH_VDEV0:=net_null0}"          # 虚拟设备 0（发送端）
: "${FASTPATH_VDEV1:=net_null1}"         # 虚拟设备 1（接收端，环回）

# ========== 流量提示 ==========
# 显示如何打流验证（实际脚本不生成流量）
: "${TRAFFIC_HINT:=Use another VM/host/scapy/pktgen to send UDP packets into the DPDK port.}"

# ==============================================
# 函数定义
# ==============================================

# ========== 时间戳函数 ==========
timestamp() {
    # 返回格式：YYYYMMDD_HHMMSS
    date +"%Y%m%d_%H%M%S"
}

# ========== 记录目录管理 ==========

# 创建新的记录目录
# 返回：records/YYYYMMDD_HHMMSS-user-space-fastpath/
new_record_dir() {
    local ts
    ts="$(timestamp)"
    echo "${PROJECT_ROOT}/records/${ts}-${PROJECT_NAME}"
}

# 找到最新的记录目录（用于追加内容）
# 如果没有记录目录，则创建新的
latest_record_dir() {
    local latest
    # 查找 records/ 下的最新目录（按名称排序，取最后一个）
    latest="$(find "${PROJECT_ROOT}/records" -maxdepth 1 -type d -name "*-${PROJECT_NAME}" 2>/dev/null | sort | tail -n 1 || true)"
    if [[ -n "${latest}" ]]; then
        echo "${latest}"
    else
        new_record_dir  # 没有记录目录时创建新的
    fi
}

# 确保记录目录存在，返回记录目录路径
# 优先级：RECORD_DIR 环境变量 > 最新的已有目录 > 新建目录
ensure_record_dir() {
    # 如果 RECORD_DIR 环境变量已设置，直接使用
    if [[ -n "${RECORD_DIR:-}" ]]; then
        mkdir -p "${RECORD_DIR}"
        echo "${RECORD_DIR}"
        return
    fi

    # 否则查找或创建记录目录
    local dir
    dir="$(latest_record_dir)"
    mkdir -p "${dir}"
    echo "${dir}"
}

# ========== 工具查找 ==========

# 检查命令是否存在
need_cmd() {
    command -v "$1" >/dev/null 2>&1
}

# 查找 dpdk-devbind 工具（DPDK 网卡绑定工具）
# 返回：dpdk-devbind 或 dpdk-devbind.py 的完整路径
find_devbind() {
    local candidates=(
        "${DPDK_DEVBIND:-}"                           # 环境变量指定
        "dpdk-devbind.py"                              # 系统命令（Python 版本）
        "dpdk-devbind"                                 # 系统命令（可能已改名）
        "/usr/share/dpdk/usertools/dpdk-devbind.py"   # Debian/Ubuntu 安装路径
        "/usr/local/share/dpdk/usertools/dpdk-devbind.py"  # 编译安装路径
        "/opt/dpdk/usertools/dpdk-devbind.py"         # 其他安装路径
    )
    local c
    for c in "${candidates[@]}"; do
        [[ -z "${c}" ]] && continue  # 跳过空值
        # 检查是否是完整路径且可执行
        if [[ "${c}" == */* && -x "${c}" ]]; then
            echo "${c}"
            return 0
        fi
        # 检查是否是系统命令
        if command -v "${c}" >/dev/null 2>&1; then
            command -v "${c}"
            return 0
        fi
    done
    return 1  # 未找到
}

# ========== 安全检查 ==========

# 检查 root 权限（DPDK 操作需要 root）
require_root_for_write() {
    if [[ "${EUID}" -ne 0 ]]; then
        echo "ERROR: this action modifies system state; please run with sudo." >&2
        exit 1
    fi
}

# 检查 DPDK PCI 不能是管理口（防止误操作导致 SSH 断开）
# 这是最关键的安全保护！
guard_not_mgmt_pci() {
    # 主网卡检查
    if [[ "${DPDK_PCI}" == "${MGMT_PCI}" ]]; then
        echo "ERROR: DPDK_PCI=${DPDK_PCI} equals management PCI ${MGMT_PCI}; refuse to continue." >&2
        exit 2
    fi
    # 第二个网卡检查（如果定义了）
    if [[ -n "${DPDK_PCI_1}" && "${DPDK_PCI_1}" == "${MGMT_PCI}" ]]; then
        echo "ERROR: DPDK_PCI_1=${DPDK_PCI_1} equals management PCI ${MGMT_PCI}; refuse to continue." >&2
        exit 2
    fi
}

# 检查 APP_BIN 是否存在且可执行
require_app_bin() {
    if [[ ! -x "${APP_BIN}" ]]; then
        echo "ERROR: APP_BIN not found or not executable: ${APP_BIN}" >&2
        echo "Run: ./scripts/01_build_app.sh" >&2
        exit 1
    fi
}

# ========== 命令记录 ==========

# 将命令追加到 COMMANDS.md（格式化为可重放的 bash 代码块）
# 用法：append_command_log "${RECORD_DIR}" "sudo" "${CMD[@]}"
append_command_log() {
    local record_dir="$1"
    shift  # 移除第一个参数（record_dir），剩余的是命令
    {
        echo
        echo "## $(date '+%F %T')"  # 时间戳标题
        echo '```bash'
        printf '%q ' "$@"  # %q 格式化，保留特殊字符
        echo
        echo '```'
    } >> "${record_dir}/COMMANDS.md"
}

# ========== 环境打印 ==========

# 打印所有项目环境变量（用于日志）
print_project_env() {
    cat <<EOF2
PROJECT_ROOT=${PROJECT_ROOT}
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
FASTPATH_LCORES=${FASTPATH_LCORES}
FASTPATH_MEMORY_CHANNELS=${FASTPATH_MEMORY_CHANNELS}
FASTPATH_FILE_PREFIX=${FASTPATH_FILE_PREFIX}
FASTPATH_RUN_SECONDS=${FASTPATH_RUN_SECONDS}
FASTPATH_STATS_PERIOD=${FASTPATH_STATS_PERIOD}
FASTPATH_BURST_SIZE=${FASTPATH_BURST_SIZE}
FASTPATH_PROMISC=${FASTPATH_PROMISC}
FASTPATH_UDP_ONLY=${FASTPATH_UDP_ONLY}
FASTPATH_SWAP_MAC=${FASTPATH_SWAP_MAC}
FASTPATH_REWRITE_ENABLE=${FASTPATH_REWRITE_ENABLE}
FASTPATH_EXTRA_APP_ARGS=${FASTPATH_EXTRA_APP_ARGS}
FASTPATH_VDEV0=${FASTPATH_VDEV0}
FASTPATH_VDEV1=${FASTPATH_VDEV1}
EOF2
}

# ========== 记录文件初始化 ==========

# 初始化记录目录中的文件（COMMANDS.md、SUMMARY.md、RESULT.md）
init_record_files() {
    local record_dir="$1"
    mkdir -p "${record_dir}"

    # COMMANDS.md - 记录执行的命令
    [[ -f "${record_dir}/COMMANDS.md" ]] || cat > "${record_dir}/COMMANDS.md" <<'EOC'
# COMMANDS

EOC

    # SUMMARY.md - 项目总结（模板）
    [[ -f "${record_dir}/SUMMARY.md" ]] || cat > "${record_dir}/SUMMARY.md" <<EOF2
# SUMMARY

## Project

project-user-space-fastpath

## 测试机环境

- Guest: Ubuntu 22.04.5 Desktop
- Kernel: Linux 6.8.0-110-generic
- 管理网卡: ${MGMT_IF}
- DPDK 网卡: ${DPDK_IF}
- DPDK PCI: ${DPDK_PCI}
- 默认 DPDK driver: ${DPDK_DRIVER}

## 目标

- 编译 fastpath-lite
- 初始化 DPDK EAL / mempool / ethdev / queue
- 进入 poll-mode fastpath loop
- 识别 ARP / IPv4 / UDP / non-UDP
- 可选 UDP-only 过滤
- 可选 MAC / IPv4 / UDP port rewrite
- 输出软件 stats 与 rte_eth_stats

## 结果

- 待填写

## 问题

- 待填写

## 下一步

- 待填写
EOF2

    # RESULT.md - 测试结果（模板）
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

# ========== 大页内存配置 ==========

# 配置大页内存（DPDK EAL 需要）
# 1. 创建挂载点目录
# 2. 挂载 hugetlbfs 文件系统
# 3. 设置大页数量（1024 × 2MB = 2GB）
setup_hugepages() {
    mkdir -p "${HUGEPAGE_MOUNT}"
    # 如果未挂载，则挂载 hugetlbfs
    if ! mountpoint -q "${HUGEPAGE_MOUNT}"; then
        mount -t hugetlbfs nodev "${HUGEPAGE_MOUNT}"
    fi
    # 设置大页数量（2MB × 1024 = 2GB）
    echo "${HUGEPAGES}" > /proc/sys/vm/nr_hugepages
}

# ========== 应用参数生成 ==========

# 生成 fastpath-lite 的应用层参数
# 根据环境变量 FASTPATH_UDP_ONLY、FASTPATH_REWRITE_ENABLE 等生成对应参数
# 用于 03_run_fastpath_single_port.sh 等脚本中
base_app_args() {
    printf '%s ' \
        --run-seconds "${FASTPATH_RUN_SECONDS}" \
        --stats-period "${FASTPATH_STATS_PERIOD}" \
        --burst-size "${FASTPATH_BURST_SIZE}" \
        --promisc "${FASTPATH_PROMISC}" \
        --udp-only "${FASTPATH_UDP_ONLY}" \
        --swap-mac "${FASTPATH_SWAP_MAC}" \
        --rewrite "${FASTPATH_REWRITE_ENABLE}"
    # 如果有额外参数（06_run_fastpath_rewrite_demo.sh 设置的 rewrite 参数），也添加
    if [[ -n "${FASTPATH_EXTRA_APP_ARGS}" ]]; then
        printf '%s ' ${FASTPATH_EXTRA_APP_ARGS}
    fi
}