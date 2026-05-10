#!/usr/bin/env bash
#============================================================
# 00_check_env.sh — 环境检查脚本
#
# 功能：
#   检查 AF_XDP 实验所需的工具链、库、头文件、网卡状态，
#   将结果写入 records/<时间戳>/ENV_CHECK.txt。
#
# 使用：
#   ./scripts/00_check_env.sh
#
# 需要检查的内容：
#   - clang / cc / make / pkg-config（编译基础）
#   - bpftool / ip / ethtool / lspci（内核/网卡调试）
#   - libbpf.pc 和 bpf/xsk 头文件（AF_XDP 编程接口）
#   - 实验网卡是否存在、驱动、队列数
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 记录目录由 common.sh 中的 latest_record_dir() 管理
REC_DIR="$(make_record_dir)"
OUT="${REC_DIR}/ENV_CHECK.txt"

{
    # 1. 输出环境变量头（接口、PCI、模式等）
    write_env_header
    echo

    # 2. 检查工具链
    echo "== toolchain =="
    for c in clang cc make pkg-config bpftool ip ethtool lspci; do
        if command -v "${c}" >/dev/null 2>&1; then
            echo "${c}: $(command -v "${c}")"
            # clang 和 bpftool 额外打印版本信息
            case "${c}" in
                clang) clang --version | head -1 ;;
                bpftool) bpftool version 2>/dev/null | head -5 || true ;;
            esac
        else
            echo "${c}: NOT_FOUND"
        fi
    done
    echo

    # 3. 检查 libbpf 开发库
    echo "== libbpf / xsk headers =="
    if pkg-config --exists libbpf 2>/dev/null; then
        echo "libbpf.pc: FOUND"
        echo "libbpf version: $(pkg-config --modversion libbpf)"
        echo "libbpf cflags: $(pkg-config --cflags libbpf)"
        echo "libbpf libs: $(pkg-config --libs libbpf)"
    else
        echo "libbpf.pc: NOT_FOUND"
        echo "hint: sudo apt install libbpf-dev libelf-dev zlib1g-dev"
    fi
    # 检查关键头文件是否存在
    for h in /usr/include/bpf/libbpf.h /usr/include/bpf/xsk.h /usr/include/bpf/bpf_helpers.h; do
        if [[ -f "${h}" ]]; then
            echo "header: ${h} FOUND"
        else
            echo "header: ${h} MISSING"
        fi
    done
    echo

    # 4. 检查实验网卡状态
    echo "== iface =="
    if [[ -d "/sys/class/net/${AF_XDP_IFACE}" ]]; then
        echo "iface ${AF_XDP_IFACE}: FOUND"
        ip -br link show "${AF_XDP_IFACE}" || true
        ip -s link show "${AF_XDP_IFACE}" || true
        ethtool -i "${AF_XDP_IFACE}" 2>/dev/null || true
        echo "queues:"
        ls -d "/sys/class/net/${AF_XDP_IFACE}/queues"/* 2>/dev/null || true
    else
        echo "iface ${AF_XDP_IFACE}: NOT_FOUND"
        echo "hint: if previous DPDK test bound ${AF_XDP_PCI} to uio/vfio, run 02_prepare_kernel_netdev.sh"
    fi
    echo

    # 5. 检查 PCI 设备信息
    echo "== pci =="
    lspci -nnk -s "${AF_XDP_PCI}" 2>/dev/null || true
    echo

    echo "RECORD_DIR=${REC_DIR}"
} 2>&1 | tee "${OUT}"