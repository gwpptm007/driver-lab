#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# =============================================================================
# run.sh — stage08 模块的加载/卸载/重载/状态查看
# =============================================================================
#
# 【学习要点】
#
# 1. insmod vs modprobe
#    insmod 直接加载一个 .ko 文件，不解析依赖。
#    modprobe 会解析 koobject 依赖图，自动先加载依赖的模块（比如 virtio.ko）。
#    这里用 insmod 是因为 stage08 是纯模拟驱动，没有真实硬件依赖。
#
# 2. 模块参数（Module Parameters）
#    insmod 时可以传递键值对参数：ifname=xxx ring_size=128
#    这些参数在驱动代码中用 module_param() / module_param_named() 声明。
#    stage08 的参数定义了：接口名、ring 大小、NAPI weight、backend 模拟延迟等。
#
# 3. NAPI weight 的意义
#    NAPI weight 决定一次 poll() 最多处理多少个 packet。
#    太大可能导致 softirq 时间过长（影响其他设备），太小可能处理不及时。
#    stage08 默认 32 是一个平衡值，和常见网卡驱动一致。
#
# 4. ring_size 和 RX buffer 的关系
#    TX/RX 各有一个环形队列，ring_size=128 表示最大 128 个 descriptor。
#    RX 还需要 rx_buf_size（每个 buffer 的大小，默认 2048 字节）。
#    virtio 规范里 descriptor table、available ring、used ring 的大小都由 ring_size 决定。
#
# 5. backend_delay_us 的模拟作用
#    设置为 > 0 时，backend worker 会在每批处理后 sleep，模拟真实的异步延迟。
#    这对测试非常重要：证明了 doorbell 触发后，backend 不需要立即执行。
#
# 6. rmmod 的安全条件
#    模块可以被卸载的前提：没有其他模块依赖它，且没有设备在使用它。
#    对于网络设备，需要先 ip link set down（或设备已关闭）。
#    如果模块正在被使用（引用计数 > 0），rmmod 会失败。
#
# 7. reload 的原子性
#    先 unload 再 load，确保旧实例完全清理后再启动新实例。
#    "|| true" 保证了即使 unload 失败（比如模块未加载），reload 也不会中断。
#
# =============================================================================

set -euo pipefail

# 【学习】时间戳用于生成唯一的日志文件名
# date +%Y%m%d-%H%M%S 生成 20260418-001506 这样的格式
STAMP=$(date +%Y%m%d-%H%M%S)

# 【学习】默认输出文件名可通过第一个参数覆盖
# 用法：./run.sh my_custom_log.log
OUT=${1:-stage08_dmesg_${STAMP}.log}

# =============================================================================
# 参数默认值（可通过环境变量覆盖）
# =============================================================================
# IFNAME: 网络接口名称，对应驱动中的 ifname 参数
IFNAME=${IFNAME:-nds8}
# RING_SIZE: TX/RX 环形队列大小，必须是 2 的幂
RING_SIZE=${RING_SIZE:-128}
# NAPI_WEIGHT: NAPI poll 每次最多处理的包数
NAPI_WEIGHT=${NAPI_WEIGHT:-32}
# RX_BUF_SIZE: 每个 RX buffer 的大小（字节）
RX_BUF_SIZE=${RX_BUF_SIZE:-2048}
# BACKEND_DELAY_US: backend worker 每批处理后模拟的延迟（微秒），0=无延迟
BACKEND_DELAY_US=${BACKEND_DELAY_US:-0}
# BACKEND_BATCH: backend 每批处理的最大 descriptor 数
BACKEND_BATCH=${BACKEND_BATCH:-32}

# =============================================================================
# is_loaded — 检查模块是否已加载
# =============================================================================
# 【学习】lsmod 输出格式：模块名 大小 依赖列表
# awk '{print $1}' 提取第一列（模块名）
# grep -qx 精确匹配整行（-q 静默，-x 整行匹配）
# 如果 netdev_stage08 在列表中，说明已加载
is_loaded() {
    lsmod | awk '{print $1}' | grep -qx netdev_stage08
}

# =============================================================================
# 主入口 — 根据 ACTION 参数选择操作
# =============================================================================
ACTION=${1:-reload}

case "$ACTION" in
    load)
        # 【学习】加载模块前检查
        # 测试阶段经常忘记先 build，此时 KO 文件不存在，提前检查避免空等
        test -f "$KO" || { echo "[stage08] missing $KO, run scripts/build.sh first" >&2; exit 1; }

        # 检查是否已经加载，避免重复加载
        if is_loaded; then
            echo "[stage08] netdev_stage08 already loaded"
            exit 0
        fi

        # 【学习】insmod 加载模块并传递参数
        # module_param 声明的参数在 insmod 时通过 key=value 传递
        # 参数类型匹配由驱动代码中的 module_param 宏指定
        # 变量后的 \ 用于续行，使参数列表更清晰
        sudo insmod "$KO" \
            ifname="$IFNAME" \
            ring_size="$RING_SIZE" \
            napi_weight="$NAPI_WEIGHT" \
            rx_buf_size="$RX_BUF_SIZE" \
            backend_delay_us="$BACKEND_DELAY_US" \
            backend_batch="$BACKEND_BATCH"

        # 【学习】加载后等待设备初始化完成
        # 驱动 probe 可能在模块加载后才完成（异步的），给 1 秒缓冲
        sleep 1

        # 【学习】启动网络接口
        # 驱动注册 netdev 后，接口存在但默认是 DOWN 状态
        # 需要手动 ip link set up 把它激活，才能收发光模块相关的 IRQ
        sudo ip link set dev "$IFNAME" up || true

        # 【学习】-details 显示队列和地址详情（MAC 地址、txqlen 等）
        # || true 是因为接口可能在某些容器环境里无法显示详细信息
        ip -details link show "$IFNAME" || true
        ;;

    unload)
        # 【学习】卸载模块
        # rmmod 不需要写 .ko 路径，只需要模块名（在 /sys/module/ 下可以找到）
        if is_loaded; then
            sudo rmmod netdev_stage08
            echo "[stage08] unloaded"
        else
            echo "[stage08] module not loaded"
        fi
        ;;

    reload)
        # 【学习】reload = 先 unload 再 load
        # 这在开发调试阶段非常常用：改代码 → build → reload 即可
        # || true 保证即使旧模块根本没加载，unload 失败也不会阻断 load
        "$0" unload || true
        "$0" load
        ;;

    status)
        # 【学习】查看当前状态：是否加载、接口信息
        is_loaded && echo "[stage08] loaded" || echo "[stage08] not loaded"
        ip -details link show "$IFNAME" || true
        ;;

    *)
        echo "Usage: $0 [load|unload|reload|status]" >&2
        exit 1
        ;;
esac