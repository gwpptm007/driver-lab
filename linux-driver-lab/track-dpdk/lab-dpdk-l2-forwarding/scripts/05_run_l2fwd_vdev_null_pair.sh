#!/usr/bin/env bash
# =============================================================================
# 05_run_l2fwd_vdev_null_pair.sh — net_null 虚拟双端口 smoke（不需要 root）
#
# 使用 DPDK 内置的 net_null PMD 创建两个虚拟端口，不依赖物理网卡。
# net_null 的特性：收包返回 0 个包，发包直接丢弃 —— 纯粹验证代码逻辑路径。
#
# 目的：
#   - 验证"双端口配对初始化"的代码路径（不需要真的有两块物理网卡）
#   - 验证端口 0 <-> 端口 1 配对逻辑
#   - 不需要 root 权限，不需要绑定物理网卡
#
# EAL 参数说明：
#   --no-pci              跳过 PCI 设备扫描（不使用任何物理网卡）
#   --vdev net_null0      创建第一个虚拟 null 端口
#   --vdev net_null1      创建第二个虚拟 null 端口
#   --promisc 0           net_null 不需要混杂模式
#
# 产出：records/<timestamp>-dpdk-l2-forwarding/L2FWD_VDEV_NULL_PAIR.log
# =============================================================================
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/L2FWD_VDEV_NULL_PAIR.log"
CMD_OUT="${RECORD_DIR}/L2FWD_VDEV_NULL_PAIR_COMMAND.txt"
: > "${OUT}"
: > "${CMD_OUT}"

if [[ ! -x "${APP_BIN}" ]]; then
    echo "ERROR: app binary not found: ${APP_BIN}" | tee -a "${OUT}"
    echo "Run: ./scripts/01_build_app.sh" | tee -a "${OUT}"
    exit 1
fi

# ── 构造虚拟端口 EAL 命令 ────────────────────────────────────────────────────
cmd=(
    "${APP_BIN}"
    -l "${L2FWD_LCORES}"
    -n "${L2FWD_MEMORY_CHANNELS}"
    --file-prefix "${L2FWD_FILE_PREFIX}_null"  # 不同的前缀，避免与物理端口实例冲突
    --no-pci                                    # 不扫描 PCI 设备
    --vdev "${L2FWD_VDEV0}"                    # 虚拟端口 0
    --vdev "${L2FWD_VDEV1}"                    # 虚拟端口 1
    --
    --run-seconds "${L2FWD_RUN_SECONDS}"
    --stats-period "${L2FWD_STATS_PERIOD}"
    --burst-size "${L2FWD_BURST_SIZE}"
    --promisc 0                                 # net_null 不需要混杂模式
)

{
    echo "# L2FWD_VDEV_NULL_PAIR_COMMAND"
    printf '%q ' "${cmd[@]}"
    echo
} > "${CMD_OUT}"
append_command_log "${RECORD_DIR}" "sudo" "${cmd[@]}"

{
    echo "# L2FWD_VDEV_NULL_PAIR"
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

echo "[OK] l2fwd vdev null-pair log: ${OUT}"
