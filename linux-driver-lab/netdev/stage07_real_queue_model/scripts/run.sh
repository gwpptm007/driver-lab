#!/usr/bin/env bash
# ================================================================================
# stage07 run.sh — 模块加载/卸载/状态查看脚本
#
# 【功能】
# - load  : 加载 netdev_stage07.ko 模块，设置 netdev up
# - unload: 卸载模块
# - reload : 卸载后重新加载（调试常用）
# - status : 查看模块是否已加载
#
# 【用法】
#   ./scripts/run.sh load       # 加载模块
#   ./scripts/run.sh unload    # 卸载模块
#   ./scripts/run.sh reload    # 重新加载（调试用）
#   ./scripts/run.sh status    # 查看加载状态
#
# 【环境变量】
# - IFNAME     : net_device 名称（默认 nds7）
# - RING_SIZE  : TX/RX queue 深度（默认 128）
# - NAPI_WEIGHT: NAPI poll weight（默认 32）
# - RX_BUF_SIZE: RX buffer 大小（默认 2048）
#
# 【示例】
#   # 使用非默认参数加载
#   IFNAME=mydev RING_SIZE=64 ./scripts/run.sh load
#
# 【加载后的状态】
# - 模块加载后，ifconfig 或 ip link 可看到 nds7 设备
# - 设备默认状态 DOWN，需要 ip link set nds7 up 才可收发包
# - debugfs 路径: /sys/kernel/debug/netdev_stage07/stats 和 queues
# ================================================================================

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
KO="$ROOT_DIR/output/netdev_stage07.ko"

# ACTION: 支持 load / unload / reload / status 四个动作
ACTION=${1:-reload}

# 模块参数（默认值）
IFNAME=${IFNAME:-nds7}
RING_SIZE=${RING_SIZE:-128}
NAPI_WEIGHT=${NAPI_WEIGHT:-32}
RX_BUF_SIZE=${RX_BUF_SIZE:-2048}

# is_loaded: 检查模块是否已加载
#   lsmod 输出格式: ModuleName Size Used by
#   awk '{print $1}' 取第一列（模块名）
#   grep -qx netdev_stage07 精确匹配整行
is_loaded() {
    lsmod | awk '{print $1}' | grep -qx netdev_stage07
}

case "$ACTION" in
    load)
        # 检查 .ko 文件是否存在
        test -f "$KO" || { echo "[stage07] missing $KO, run scripts/build.sh first" >&2; exit 1; }

        # 如果模块已加载，提示并退出（避免重复加载）
        if is_loaded; then
            echo "[stage07] netdev_stage07 already loaded"
            exit 0
        fi

        # insmod 加载模块，传入参数
        #   ifname      : 设备名（默认 nds7）
        #   ring_size   : queue 深度（默认 128）
        #   napi_weight : NAPI poll weight（默认 32）
        #   rx_buf_size : RX buffer 大小（默认 2048）
        sudo insmod "$KO" ifname="$IFNAME" ring_size="$RING_SIZE" napi_weight="$NAPI_WEIGHT" rx_buf_size="$RX_BUF_SIZE"

        sleep 1  # 等待模块初始化完成

        # 设置设备 UP（协议栈才能收发包）
        sudo ip link set dev "$IFNAME" up || true

        # 显示设备详细信息（确认设备已注册）
        ip -details link show "$IFNAME" || true
        ;;

    unload)
        # 卸载模块（rmmod 会调用 module_exit）
        # 如果模块未加载，提示并退出
        if is_loaded; then
            sudo rmmod netdev_stage07
            echo "[stage07] unloaded"
        else
            echo "[stage07] module not loaded"
        fi
        ;;

    reload)
        # 重新加载：先卸载，再加载
        # 注意：reload 会重置所有统计计数
        "$0" unload || true
        "$0" load
        ;;

    status)
        # 查看模块加载状态和设备信息
        is_loaded && echo "[stage07] loaded" || echo "[stage07] not loaded"
        ip -details link show "$IFNAME" || true
        ;;

    *)
        echo "Usage: $0 [load|unload|reload|status]" >&2
        exit 1
        ;;
esac
