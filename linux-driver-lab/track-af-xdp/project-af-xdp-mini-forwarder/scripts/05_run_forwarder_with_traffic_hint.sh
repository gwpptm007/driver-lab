#!/usr/bin/env bash
#============================================================
# 05_run_forwarder_with_traffic_hint.sh — 发包指引脚本
#
# 功能：
#   生成发包指引，告知如何在实验期间向实验机发包，
#   以触发 XDP redirect 并满足 PASS_TRAFFIC。
#
# 原理：
#   转发器本身是被动收包，没有流量时 rx_packets 为 0。
#   需要从另一台主机向实验机的 AF_XDP 接口发包。
#
# 发包方式（任选一种）：
#   1. ping — 简单但需要 IP 配置
#   2. Python UDP 广播 — 脚本化、可控
#
# 通过标准：
#   rx_packets > 0（reflect 模式下 tx_packets/comp_packets 也应 > 0）
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="$(latest_record_dir)"
out="${record_dir}/TRAFFIC_HINT.txt"

{
    write_env_header
    echo
    echo "Run traffic while the forwarder is running. Example from peer VM/host:"
    echo
    echo "方式一：ping（需先给实验机配置 IP）"
    echo "  ping <ens192 的 IP>"
    echo
    echo "方式二：Python UDP 发包（不需要 IP，广播即可）"
    echo "  python3 -c 'import socket; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); [s.sendto(b\"af-xdp\",(\"<目标 IP>\",9000)) for _ in range(1000)]'"
    echo
    echo "预期结果：FORWARDER_FINAL_STATS 中 rx_packets > 0。"
    echo "reflect 模式下，tx_packets 和 comp_packets 也应增加。"
} | tee "${out}"