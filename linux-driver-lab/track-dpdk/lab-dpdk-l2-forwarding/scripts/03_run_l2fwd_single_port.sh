#!/usr/bin/env bash
# =============================================================================
# 03_run_l2fwd_single_port.sh — 单端口 smoke 测试（需要 root）
#
# 当前测试机只有一块 VMXNET3 DPDK 网卡，无法做真正的双端口 L2 转发。
# 这个脚本验证 l2fwd-lite 的基本数据面骨架：
#   - EAL 初始化
#   - mempool / ethdev / queue 初始化
#   - 进入 rx_burst 轮询循环
#   - 单端口模式下收到的包直接 free（smoke 模式）
#   - 定时打印软件 stats 和硬件 stats
#
# EAL 参数说明：
#   -l 0-1                  使用 lcore 0 和 1
#   -n 4                    4 个内存通道
#   --file-prefix xxx       DPDK 多实例文件前缀
#   -a 0000:0b:00.0         只使用这块 PCI 设备
#   --                      EAL 参数结束，后面是应用参数
#   --run-seconds 15        运行 15 秒后自动退出
#   --stats-period 2        每 2 秒打印一次统计
#   --burst-size 32         每次 burst 收发 32 个包
#   --promisc 1             开启混杂模式
#
# 产出：records/<timestamp>-dpdk-l2-forwarding/L2FWD_SINGLE_PORT.log
# =============================================================================
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/L2FWD_SINGLE_PORT.log"
CMD_OUT="${RECORD_DIR}/L2FWD_SINGLE_PORT_COMMAND.txt"
: > "${OUT}"
: > "${CMD_OUT}"

# 检查编译产物是否存在
if [[ ! -x "${APP_BIN}" ]]; then
    echo "ERROR: app binary not found: ${APP_BIN}" | tee -a "${OUT}"
    echo "Run: ./scripts/01_build_app.sh" | tee -a "${OUT}"
    exit 1
fi

# 安全检查：确保不会误操作管理网卡
guard_not_mgmt_pci

# ── 构造 EAL + 应用参数 ──────────────────────────────────────────────────────
cmd=(
    "${APP_BIN}"
    -l "${L2FWD_LCORES}"              # lcore 范围
    -n "${L2FWD_MEMORY_CHANNELS}"     # 内存通道数
    --file-prefix "${L2FWD_FILE_PREFIX}" # 文件前缀
    -a "${DPDK_PCI}"                  # 绑定 DPDK PCI 设备
    --                                 # EAL 参数到此结束
    --run-seconds "${L2FWD_RUN_SECONDS}"  # 运行时长
    --stats-period "${L2FWD_STATS_PERIOD}" # 统计间隔
    --burst-size "${L2FWD_BURST_SIZE}"    # burst 大小
    --promisc "${L2FWD_PROMISC}"          # 混杂模式
)

# 保存完整的命令行（调试用）
{
    echo "# L2FWD_SINGLE_PORT_COMMAND"
    printf '%q ' "${cmd[@]}"
    echo
} > "${CMD_OUT}"
append_command_log "${RECORD_DIR}" "sudo" "${cmd[@]}"

# ── 执行 l2fwd-lite ──────────────────────────────────────────────────────────
{
    echo "# L2FWD_SINGLE_PORT"
    echo
    echo "## command"
    cat "${CMD_OUT}"
    echo
    echo "## run"
    "${cmd[@]}"
    rc=$?
    echo
    echo "rc=${rc}"
    exit "${rc}"
} >> "${OUT}" 2>&1

echo "[OK] l2fwd single-port log: ${OUT}"
