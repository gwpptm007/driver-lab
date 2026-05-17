#!/usr/bin/env bash
# =============================================================================
# 00_check_env.sh — 环境检查（只读，不修改系统）
#
# 检查项：
#   1. lab 环境变量是否正确加载
#   2. 操作系统和内核版本
#   3. CPU 信息
#   4. 必要的工具链（gcc, meson, ninja, dpdk-devbind 等）
#   5. libdpdk 开发库是否可用
#   6. hugepage 状态
#   7. 网卡接口和 PCI 设备
#   8. dpdk-devbind 当前绑定状态
#
# 产出：records/<timestamp>-dpdk-l2-forwarding/ENV_CHECK.txt
# =============================================================================
set -euo pipefail

# 加载公共变量和函数
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 创建新的记录目录（每次检查都创建新目录）
RECORD_DIR="$(new_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/ENV_CHECK.txt"

# 所有输出重定向到 ENV_CHECK.txt
{
    echo "# ENV_CHECK"
    echo
    # ── lab 环境变量 ─────────────────────────────────────────
    echo "## lab env"
    print_lab_env
    echo
    # ── 系统时间 ─────────────────────────────────────────────
    echo "## date"
    date -Is
    echo
    # ── 操作系统版本 ─────────────────────────────────────────
    echo "## os-release"
    cat /etc/os-release 2>/dev/null || true
    echo
    # ── 内核版本 ─────────────────────────────────────────────
    echo "## kernel"
    uname -a
    echo
    # ── CPU 信息 ─────────────────────────────────────────────
    echo "## cpu"
    nproc || true
    lscpu 2>/dev/null | sed -n '1,80p' || true
    echo
    # ── 工具链检查 ───────────────────────────────────────────
    # 逐个检查 DPDK 开发和调试需要的命令行工具
    echo "## commands"
    for c in gcc pkg-config meson ninja dpdk-devbind.py dpdk-devbind dpdk-testpmd testpmd lspci ethtool ip; do
        if command -v "$c" >/dev/null 2>&1; then
            echo "$c: $(command -v "$c")"
        else
            echo "$c: NOT FOUND"
        fi
    done
    echo
    # ── libdpdk 开发库 ───────────────────────────────────────
    # pkg-config 检查 DPDK 开发头文件和链接库是否安装
    echo "## pkg-config libdpdk"
    if pkg-config --exists libdpdk; then
        echo "libdpdk: FOUND"
        echo "version: $(pkg-config --modversion libdpdk 2>/dev/null || true)"
        echo "cflags: $(pkg-config --cflags libdpdk 2>/dev/null || true)"
        echo "libs: $(pkg-config --libs libdpdk 2>/dev/null | cut -c1-240 || true) ..."
    else
        echo "libdpdk: NOT FOUND"
    fi
    echo
    # ── Hugepage 状态 ────────────────────────────────────────
    # DPDK 依赖大页内存，检查当前系统的大页分配情况
    echo "## hugepages"
    grep -E 'HugePages|Hugepagesize|Hugetlb' /proc/meminfo || true
    echo
    # ── 网络接口 ─────────────────────────────────────────────
    echo "## interfaces"
    ip -br addr || true
    echo
    # ── DPDK 网卡驱动信息 ────────────────────────────────────
    echo "## ${DPDK_IF} ethtool"
    ethtool -i "${DPDK_IF}" 2>/dev/null || true
    echo
    # ── 管理网卡驱动信息 ─────────────────────────────────────
    echo "## ${MGMT_IF} ethtool"
    ethtool -i "${MGMT_IF}" 2>/dev/null || true
    echo
    # ── PCI 网卡设备列表 ─────────────────────────────────────
    echo "## pci"
    lspci -nn | grep -Ei 'ethernet|network|vmxnet|virtio' || true
    echo
    # ── dpdk-devbind 绑定状态 ────────────────────────────────
    # 显示每个网卡当前绑定的驱动（内核驱动 vs DPDK 用户态驱动）
    echo "## dpdk-devbind status"
    if devbind="$(find_devbind 2>/dev/null)"; then
        "${devbind}" --status || true
    else
        echo "dpdk-devbind: NOT FOUND"
    fi
} > "${OUT}" 2>&1

# 记录本次执行的命令到 COMMANDS.md
append_command_log "${RECORD_DIR}" "$0"

echo "[OK] env check saved: ${OUT}"
echo "[OK] record dir: ${RECORD_DIR}"
