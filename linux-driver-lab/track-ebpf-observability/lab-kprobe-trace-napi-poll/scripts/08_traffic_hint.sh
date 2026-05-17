#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
cat <<EOF
# Traffic hint for ${LAB_NAME}

当前目标接口：${EBPF_IFACE}

在另一个窗口运行观测脚本后，可以用下面方式制造流量：

1. 管理口/目标口已有 IP 时：
   ping <${EBPF_IFACE} 对端 IP>
   ssh/iperf/UDP 都可以

2. 查看接口是否有 RX/TX 增长：
   ip -s link show dev ${EBPF_IFACE}

3. 如果 ${EBPF_IFACE} 没有业务流量，可以临时切到管理口 smoke：
   EBPF_IFACE=${EBPF_MGMT_IFACE} sudo ./scripts/02_run_napi_poll_kprobe.sh
   EBPF_IFACE=${EBPF_MGMT_IFACE} sudo ./scripts/05_run_softirq_napi_correlation.sh

注意：NAPI poll 是系统级函数，按 CPU/comm 统计，不一定能直接精确过滤到某个 iface。
EOF
