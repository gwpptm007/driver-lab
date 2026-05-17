#!/usr/bin/env bash
# =============================================================================
# 04_run_l2fwd_two_port.sh — 双端口 L2 转发测试（需要 root）
#
# 与 03 类似，但使用两块物理 DPDK 网卡，可以做真正的 L2 转发：
#   - 端口 0 收到的包 → 交换 MAC 地址 → 从端口 1 发出
#   - 端口 1 收到的包 → 交换 MAC 地址 → 从端口 0 发出
#
# 前提条件：
#   - 需要设置环境变量 DPDK_PCI_1 指定第二块网卡的 PCI 地址
#   - 两块网卡都需要绑定到 DPDK 驱动（先跑 02_prepare_vmxnet3.sh）
#
# 用法：
#   sudo DPDK_PCI_1=0000:13:00.0 ./scripts/04_run_l2fwd_two_port.sh
#
# 产出：records/<timestamp>-dpdk-l2-forwarding/L2FWD_TWO_PORT.log
# =============================================================================
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/L2FWD_TWO_PORT.log"
CMD_OUT="${RECORD_DIR}/L2FWD_TWO_PORT_COMMAND.txt"
: > "${OUT}"
: > "${CMD_OUT}"

if [[ ! -x "${APP_BIN}" ]]; then
    echo "ERROR: app binary not found: ${APP_BIN}" | tee -a "${OUT}"
    echo "Run: ./scripts/01_build_app.sh" | tee -a "${OUT}"
    exit 1
fi

# 检查第二块 DPDK 网卡的 PCI 地址是否已设置
if [[ -z "${DPDK_PCI_1}" ]]; then
    echo "ERROR: DPDK_PCI_1 is empty. Example:" | tee -a "${OUT}"
    echo "  sudo DPDK_PCI_1=0000:xx:yy.z ./scripts/04_run_l2fwd_two_port.sh" | tee -a "${OUT}"
    exit 1
fi

guard_not_mgmt_pci

# ── 构造双端口 EAL 命令 ─────────────────────────────────────────────────────
# 与单端口相比，多了一个 -a 参数绑定第二块 PCI 设备
cmd=(
    "${APP_BIN}"
    -l "${L2FWD_LCORES}"
    -n "${L2FWD_MEMORY_CHANNELS}"
    --file-prefix "${L2FWD_FILE_PREFIX}"
    -a "${DPDK_PCI}"                  # 第一块 DPDK 网卡
    -a "${DPDK_PCI_1}"               # 第二块 DPDK 网卡
    --
    --run-seconds "${L2FWD_RUN_SECONDS}"
    --stats-period "${L2FWD_STATS_PERIOD}"
    --burst-size "${L2FWD_BURST_SIZE}"
    --promisc "${L2FWD_PROMISC}"
)

{
    echo "# L2FWD_TWO_PORT_COMMAND"
    printf '%q ' "${cmd[@]}"
    echo
} > "${CMD_OUT}"
append_command_log "${RECORD_DIR}" "sudo" "${cmd[@]}"

{
    echo "# L2FWD_TWO_PORT"
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

echo "[OK] l2fwd two-port log: ${OUT}"
