#!/usr/bin/env bash
#===============================================================================
# 03_run_single_port_smoke.sh - 单端口 smoke 测试
# 作用：用一块 DPDK 网卡启动 media-gateway-lite，验证程序能正常初始化
# 用途：快速验证编译 + EAL init + 端口启动是否正常
# 注意：不指定规则，单端口仅做 RX/TX 自环测试
#===============================================================================
set -euo pipefail

# 引入公共变量和函数
source "$(dirname "$0")/common.sh"

# 检查二进制文件是否已编译
require_media_bin

# 输出文件
OUT="${RECORD_DIR}/MEDIA_GATEWAY_SINGLE_PORT.log"           # 程序运行日志
CMD_OUT="${RECORD_DIR}/MEDIA_GATEWAY_SINGLE_PORT_COMMAND.txt" # 实际执行的命令（便于复现）

#----------------------------------------
# 构建 DPDK EAL 参数 + APP 参数
# EAL 参数（EAL 是 DPDK 基础库，负责大页、内存、网卡管理）：
#   -l: 使用的 lcore（如 0-1）
#   -n: 内存通道数（影响内存分配性能）
#   --file-prefix: 多实例隔离（同一机器上运行多个 DPDK 程序需不同 prefix）
#   -a: 添加 PCI 设备（DPDK 控制的网卡）
# APP 参数（媒体网关配置）：
#   --run-seconds: 运行时间（0=无限）
#   --stats-period: 统计打印周期
#   --burst-size: RX/TX burst 大小
#   --promisc: 混杂模式开关
#   --udp-only: 是否只处理 UDP 流量
#   --swap-mac: 无 MAC rewrite 时是否交换 src/dst MAC
#   --strict-rules: 严格模式（无匹配规则则丢弃）
#----------------------------------------
CMD=("${MEDIA_BIN}"
  -l "${MEDIA_LCORES}"
  -n "${MEDIA_MEMORY_CHANNELS}"
  --file-prefix "${MEDIA_FILE_PREFIX}_single"   # 单端口实例隔离
  -a "${DPDK_PCI}"                              # 绑定 PCIe 网卡
  --
  --run-seconds "${MEDIA_RUN_SECONDS}"
  --stats-period "${MEDIA_STATS_PERIOD}"
  --burst-size "${MEDIA_BURST_SIZE}"
  --promisc "${MEDIA_PROMISC}"
  --udp-only "${MEDIA_UDP_ONLY}"
  --swap-mac "${MEDIA_SWAP_MAC}"
  --strict-rules "${MEDIA_STRICT_RULES}"
)

# 支持通过 MEDIA_EXTRA_APP_ARGS 传入额外参数（如额外的 --rule 配置）
if [[ -n "${MEDIA_EXTRA_APP_ARGS}" ]]; then
  # shellcheck disable=SC2206
  EXTRA_ARGS=( ${MEDIA_EXTRA_APP_ARGS} )
  CMD+=("${EXTRA_ARGS[@]}")
fi

# 打印实际执行的命令到日志（便于复现）
printf '%q ' "${CMD[@]}" | tee "${CMD_OUT}"
echo | tee -a "${CMD_OUT}"

# 执行并捕获输出
"${CMD[@]}" 2>&1 | tee "${OUT}"

echo "[OK] single-port smoke saved: ${OUT}"