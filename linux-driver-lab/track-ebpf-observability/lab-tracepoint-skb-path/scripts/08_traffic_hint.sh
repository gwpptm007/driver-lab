#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
cat <<EOF
# Traffic hint for ${LAB_NAME}

当前目标接口：${EBPF_IFACE}

tracepoint 观测需要真实流量才能触发事件。在运行观测脚本前，另开窗口制造流量：

1. 基础 ICMP 流量：
   ping -i 0.2 <网关或对端 IP>

2. TCP/UDP 流量：
   iperf3 -c <对端 IP> -t 20
   或 nc -u <对端 IP> 9999

3. 确认接口有流量增长：
   watch -n1 "ip -s link show dev ${EBPF_IFACE}"

4. 如果 ${EBPF_IFACE} 没有业务流量，切到管理口：
   EBPF_IFACE=${EBPF_MGMT_IFACE} sudo ./scripts/02_run_skb_rx_trace.sh
   EBPF_IFACE=${EBPF_MGMT_IFACE} sudo ./scripts/05_run_skb_full_path.sh

注意：tracepoint 是内核 ABI，比 kprobe 更稳定。同一个 tracepoint:net:netif_receive_skb
在 5.x/6.x 内核上字段名完全一致，不需要像 kprobe 那样 fallback 多个符号名。
EOF
