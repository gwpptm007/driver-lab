#!/usr/bin/env bash
#============================================================
# 04_run_af_xdp_rx_with_traffic_hint.sh — 流量测试提示脚本
#
# 功能：
#   生成发包指引，告知如何在实验期间从另一台主机向实验机发包，
#   以触发 XDP redirect 并满足 PASS_RX_TRAFFIC 标准。
#
# 原理：
#   AF_XDP socket 本身是被动收包，如果没有流量经过实验网卡的队列，
#   poll() 永远没有数据，rx_packets 会一直是 0。
#   因此需要从对端主机主动发包到实验机的 AF_XDP 接口。
#
# 发包方式（任选一种）：
#   1. ping — 简单但需 IP 配置
#   2. arping — ARP 层发包，不需要 IP
#   3. Python UDP broadcast — 脚本化、可控
#
# 通过标准：
#   AF_XDP_FINAL_STATS 中 rx_packets > 0
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

REC_DIR="$(latest_record_dir)"
OUT="${REC_DIR}/TRAFFIC_HINT.txt"

{
    write_env_header
    echo
    echo "This lab needs packets arriving on ${AF_XDP_IFACE} queue ${AF_XDP_QUEUE}."
    echo
    echo "Recommended procedure:"
    echo
    echo "1. 终端 A：在测试机上启动 AF_XDP 收包程序（后台运行）"
    echo "   cd ${LAB_DIR}"
    echo "   sudo AF_XDP_DURATION=30 ./scripts/03_run_af_xdp_socket_smoke.sh"
    echo
    echo "2. 终端 B/C：在同 VMware 网络的另一台主机上发包"
    echo "   方式一：ping（需先配 IP）"
    echo "     ping <实验机 ens192 的 IP>"
    echo
    echo "   方式二：arping（ARP 层，不需要 IP）"
    echo "     arping -I <对端网卡> <实验机 ens192 的 IP>"
    echo
    echo "   方式三：Python UDP 广播（可控、速率可调）"
    echo "     python3 - <<'PY'"
    echo "import socket, time"
    echo "dst=('255.255.255.255', 9000)"
    echo "s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)"
    echo "s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)"
    echo "for i in range(1000):"
    echo "    s.sendto(b'af-xdp-test-%d' % i, dst)"
    echo "    time.sleep(0.002)"
    echo "PY"
    echo
    echo "3. 回到终端 A：确认 AF_XDP_FINAL_STATS 中 rx_packets > 0"
} | tee "${OUT}"