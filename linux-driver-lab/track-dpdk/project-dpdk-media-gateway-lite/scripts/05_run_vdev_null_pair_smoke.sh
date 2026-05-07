#!/usr/bin/env bash
#===============================================================================
# 05_run_vdev_null_pair_smoke.sh - vdev null pair smoke 测试
# 作用：使用虚拟设备（net_null0 + net_null1）启动 media-gateway-lite
# 用途：
#   - 无需真实网卡，在任何机器上都能运行
#   - 验证程序逻辑（EAL init、端口初始化、规则匹配）是否正常
#   - null 设备会丢弃所有发出去的包，不会真正发送出去
# 注意：vdev null pair 发出的包会被直接丢弃，不会触发 RX，因此看不到 rx 统计
#===============================================================================
set -euo pipefail

# 引入公共变量和函数
source "$(dirname "$0")/common.sh"

# 检查二进制文件
require_media_bin

# 输出文件
OUT="${RECORD_DIR}/MEDIA_GATEWAY_VDEV_NULL_PAIR.log"
CMD_OUT="${RECORD_DIR}/MEDIA_GATEWAY_VDEV_NULL_PAIR_COMMAND.txt"

#----------------------------------------
# 构建命令（与单端口类似，但用虚拟设备代替物理网卡）
# --no-pci: 不扫描 PCI 设备
# --vdev net_null0: 创建虚拟网卡 0（所有发往此网卡的包都会被丢弃）
# --vdev net_null1: 创建虚拟网卡 1
# 注意：vdev null pair 无法收到任何包（TX 后直接丢弃），所以不会有 rx 计数
#      这是预期行为，不代表程序有问题
#----------------------------------------
CMD=("${MEDIA_BIN}"
  -l "${MEDIA_LCORES}"
  -n "${MEDIA_MEMORY_CHANNELS}"
  --file-prefix "${MEDIA_FILE_PREFIX}_vdev_null"
  --no-pci                               # 不扫描物理 PCI 网卡
  --vdev net_null0                       # 虚拟设备 0（发往此设备的包被丢弃）
  --vdev net_null1                       # 虚拟设备 1（发往此设备的包被丢弃）
  --
  --run-seconds "${MEDIA_RUN_SECONDS}"
  --stats-period "${MEDIA_STATS_PERIOD}"
  --burst-size "${MEDIA_BURST_SIZE}"
  --promisc "${MEDIA_PROMISC}"
  --udp-only "${MEDIA_UDP_ONLY}"
  --swap-mac "${MEDIA_SWAP_MAC}"
  --strict-rules "${MEDIA_STRICT_RULES}"
  --rule0 "${RULE0_DIR}"                # port0 → port1 方向
  --rule0-name "${RULE0_NAME}"
  --rule1 "${RULE1_DIR}"                # port1 → port0 方向
  --rule1-name "${RULE1_NAME}"
)

# 打印命令并执行
printf '%q ' "${CMD[@]}" | tee "${CMD_OUT}"
echo | tee -a "${CMD_OUT}"
"${CMD[@]}" 2>&1 | tee "${OUT}"

echo "[OK] vdev null pair smoke saved: ${OUT}"