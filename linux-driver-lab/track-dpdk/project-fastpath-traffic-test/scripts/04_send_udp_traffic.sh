#!/usr/bin/env bash
#===============================================================================
# 04_send_udp_traffic.sh - 生成/发送 UDP 测试流量
# 作用：生成 scapy 发包参考命令（默认只打印不发送）
#      真实发包需要从外部 VM/宿主机执行
# 输出：records/<tag>/UDP_SENDER_HINT.txt
#===============================================================================
source "$(dirname "$0")/common.sh"

PRINT_ONLY=0
if [[ "${1:-}" == "--print-only" ]]; then
  PRINT_ONLY=1
fi

OUT="${RECORD_DIR}/UDP_SENDER_HINT.txt"
{
  echo "# UDP_SENDER_HINT"
  echo
  echo "本脚本默认只生成外部发包参考命令。"
  echo
  echo "原因：当 ${DPDK_IF}/${DPDK_PCI} 绑定到 DPDK driver 后，当前 guest 的 Linux kernel 不再拥有该接口。"
  echo "真实流量应从另一台 VM、宿主机或后续 vhost/virtio-user 拓扑发送。"
  echo
  echo "## example"
  echo
  cat <<EOC
python3 tools/scapy_udp_sender.py \\
  --iface <sender-iface> \\
  --dst-mac <fastpath-port0-mac> \\
  --dst-ip ${SENDER_DST_IP} \\
  --dst-port ${SENDER_DST_PORT} \\
  --count ${SENDER_COUNT}
EOC
  echo
  echo "## current env"
  echo "SENDER_IF=${SENDER_IF}"
  echo "SENDER_DST_MAC=${SENDER_DST_MAC}"
  echo "SENDER_DST_IP=${SENDER_DST_IP}"
  echo "SENDER_DST_PORT=${SENDER_DST_PORT}"
  echo "SENDER_COUNT=${SENDER_COUNT}"
} | tee "${OUT}"

if [[ "${PRINT_ONLY}" -eq 1 ]]; then
  exit 0
fi

if [[ -z "${SENDER_IF}" || -z "${SENDER_DST_MAC}" ]]; then
  echo "[ERR] local sender requires SENDER_IF and SENDER_DST_MAC; usually this should run on external sender machine." >&2
  exit 1
fi

python3 "${PROJECT_ROOT}/tools/scapy_udp_sender.py" \
  --iface "${SENDER_IF}" \
  --dst-mac "${SENDER_DST_MAC}" \
  --dst-ip "${SENDER_DST_IP}" \
  --dst-port "${SENDER_DST_PORT}" \
  --count "${SENDER_COUNT}"
