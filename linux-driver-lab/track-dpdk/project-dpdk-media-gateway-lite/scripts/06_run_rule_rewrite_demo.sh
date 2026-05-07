#!/usr/bin/env bash
#===============================================================================
# 06_run_rule_rewrite_demo.sh - 规则 rewrite 功能演示
# 作用：使用 vdev null pair 运行，并配置带 MAC/IP/Port rewrite 的规则
# 用途：验证五元组匹配 + rewrite（改写 MAC、IP、Port）功能是否正常
# 说明：
#   - 匹配条件：in_port + 目的 UDP 端口（rule0 匹配 9000，rule1 匹配 10000）
#   - rewrite 动作：改写 dst MAC、dst IP、dst Port
#   - 转发方向：port0 ↔ port1 双向
#===============================================================================
set -euo pipefail

# 引入公共变量和函数
source "$(dirname "$0")/common.sh"

# 检查二进制文件
require_media_bin

# 输出文件
OUT="${RECORD_DIR}/MEDIA_GATEWAY_REWRITE_DEMO.log"
CMD_OUT="${RECORD_DIR}/MEDIA_GATEWAY_REWRITE_DEMO_COMMAND.txt"

#----------------------------------------
# 构建命令（双 vdev null 设备 + rewrite 规则配置）
# 规则 0：port0 → port1，匹配目的端口 9000
#   rewrite: 改写目的 MAC 为 52:54:00:00:00:02
#   rewrite: 改写目的 IP 为 10.10.20.20
#   rewrite: 改写目的 Port 为 10000
# 规则 1：port1 → port0，匹配目的端口 10000
#   rewrite: 改写目的 MAC 为 52:54:00:00:00:01
#   rewrite: 改写目的 IP 为 10.10.10.10
#   rewrite: 改写目的 Port 为 9000
#----------------------------------------
CMD=("${MEDIA_BIN}"
  -l "${MEDIA_LCORES}"
  -n "${MEDIA_MEMORY_CHANNELS}"
  --file-prefix "${MEDIA_FILE_PREFIX}_rewrite"
  --no-pci
  --vdev net_null0
  --vdev net_null1
  --
  --run-seconds "${MEDIA_RUN_SECONDS}"
  --stats-period "${MEDIA_STATS_PERIOD}"
  --burst-size "${MEDIA_BURST_SIZE}"
  --promisc "${MEDIA_PROMISC}"
  --udp-only "${MEDIA_UDP_ONLY}"
  --swap-mac "${MEDIA_SWAP_MAC}"
  --strict-rules "${MEDIA_STRICT_RULES}"
  --rule0 "${RULE0_DIR}"
  --rule0-name "${RULE0_NAME}"
  --rule0-dst-port "${RULE0_DST_PORT}"                 # 匹配目的端口 9000
  --rule0-rewrite-dst-mac "${RULE0_REWRITE_DST_MAC}"   # rewrite 目的 MAC
  --rule0-rewrite-dst-ip "${RULE0_REWRITE_DST_IP}"    # rewrite 目的 IP
  --rule0-rewrite-dst-port "${RULE0_REWRITE_DST_PORT}" # rewrite 目的 Port
  --rule1 "${RULE1_DIR}"
  --rule1-name "${RULE1_NAME}"
  --rule1-dst-port "${RULE1_DST_PORT}"                 # 匹配目的端口 10000
  --rule1-rewrite-dst-mac "${RULE1_REWRITE_DST_MAC}"  # rewrite 目的 MAC
  --rule1-rewrite-dst-ip "${RULE1_REWRITE_DST_IP}"     # rewrite 目的 IP
  --rule1-rewrite-dst-port "${RULE1_REWRITE_DST_PORT}" # rewrite 目的 Port
)

# 打印命令并执行
printf '%q ' "${CMD[@]}" | tee "${CMD_OUT}"
echo | tee -a "${CMD_OUT}"
"${CMD[@]}" 2>&1 | tee "${OUT}"

echo "[OK] rewrite demo saved: ${OUT}"