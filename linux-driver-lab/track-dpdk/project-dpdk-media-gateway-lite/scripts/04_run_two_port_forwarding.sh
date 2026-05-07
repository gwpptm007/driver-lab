#!/usr/bin/env bash
#===============================================================================
# 04_run_two_port_forwarding.sh - 双端口转发测试
# 作用：使用两块 DPDK 网卡启动 media-gateway-lite，实现端口间 L2 转发
# 用途：验证真实网卡环境下的双向转发能力
# 注意：需要设置 DPDK_PCI_0 和 DPDK_PCI_1 两个网卡 PCI 地址
#===============================================================================
set -euo pipefail

# 引入公共变量和函数
source "$(dirname "$0")/common.sh"

# 检查二进制文件
require_media_bin

#----------------------------------------
# 获取两个网卡的 PCI 地址（必须提供）
# DPDK_PCI_0: 第一个网卡（默认取 DPDK_PCI）
# DPDK_PCI_1: 第二个网卡（必须显式传入）
#----------------------------------------
: "${DPDK_PCI_0:=${DPDK_PCI}}"
: "${DPDK_PCI_1:=}"
if [[ -z "${DPDK_PCI_1}" ]]; then
  echo "[ERR] DPDK_PCI_1 is required for two-port forwarding" >&2
  echo "Example: sudo DPDK_PCI_1=0000:xx:yy.z ./scripts/04_run_two_port_forwarding.sh" >&2
  exit 1
fi

# 输出文件
OUT="${RECORD_DIR}/MEDIA_GATEWAY_TWO_PORT.log"
CMD_OUT="${RECORD_DIR}/MEDIA_GATEWAY_TWO_PORT_COMMAND.txt"

#----------------------------------------
# 构建 EAL 参数 + APP 参数
# 与单端口的区别：使用两个 -a 指定两个 PCI 设备，并配置双向规则
# 规则说明：
#   --rule0: port0 → port1 的转发规则（规则名称为 core_to_access）
#   --rule1: port1 → port0 的转发规则（规则名称为 access_to_core）
#----------------------------------------
CMD=("${MEDIA_BIN}"
  -l "${MEDIA_LCORES}"
  -n "${MEDIA_MEMORY_CHANNELS}"
  --file-prefix "${MEDIA_FILE_PREFIX}_two_port"   # 双端口实例隔离
  -a "${DPDK_PCI_0}"                              # 第一个网卡
  -a "${DPDK_PCI_1}"                              # 第二个网卡
  --
  --run-seconds "${MEDIA_RUN_SECONDS}"
  --stats-period "${MEDIA_STATS_PERIOD}"
  --burst-size "${MEDIA_BURST_SIZE}"
  --promisc "${MEDIA_PROMISC}"
  --udp-only "${MEDIA_UDP_ONLY}"
  --swap-mac "${MEDIA_SWAP_MAC}"
  --strict-rules "${MEDIA_STRICT_RULES}"
  --rule0 "${RULE0_DIR}"                          # port0 → port1 方向
  --rule0-name "${RULE0_NAME}"                    # 规则名称
  --rule1 "${RULE1_DIR}"                          # port1 → port0 方向
  --rule1-name "${RULE1_NAME}"                    # 规则名称
)

# 打印命令并执行
printf '%q ' "${CMD[@]}" | tee "${CMD_OUT}"
echo | tee -a "${CMD_OUT}"
"${CMD[@]}" 2>&1 | tee "${OUT}"

echo "[OK] two-port forwarding saved: ${OUT}"