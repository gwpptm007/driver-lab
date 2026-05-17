#!/usr/bin/env bash
#============================================================
# 08_clean_runtime.sh — 清理可能残留的 bpftrace 进程
#
# 功能：
#   bpftrace 通过 timeout 运行，理论上会自动退出。
#   但如果脚本异常中断，可能有残留进程。
#   此脚本检查并提示用户如何清理。
#
# 注意：
#   只检查，不自动 kill，避免误杀其他用户的 bpftrace 进程。
#
# 输出：
#   显示当前所有 bpftrace 进程（如果有）
#
# 使用：
#   ./scripts/08_clean_runtime.sh
#   # 如果有残留，手动 kill：
#   sudo kill <pid>
#
#   # 如果需要清理 XDP 程序：
#   sudo EBPF_CONFIRM_XDP_OFF=YES ./scripts/02_clean_xdp_if_attached.sh
#============================================================

set -euo pipefail

echo "== possible bpftrace processes =="
pgrep -a bpftrace || true

echo "If a stale bpftrace process exists, stop it manually after confirming it is yours."
echo "To kill: sudo kill <pid>"
echo "To detach old XDP program from the observed iface, use:"
echo "  sudo EBPF_CONFIRM_XDP_OFF=YES ./scripts/02_clean_xdp_if_attached.sh"