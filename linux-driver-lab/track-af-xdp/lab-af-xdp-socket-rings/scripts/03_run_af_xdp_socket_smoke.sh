#!/usr/bin/env bash
#============================================================
# 03_run_af_xdp_socket_smoke.sh — 运行 AF_XDP socket smoke 测试
#
# 功能：
#   执行 af_xdp_rings 程序，完成 AF_XDP socket 的最小闭环：
#   UMEM 创建 → socket 创建 → XDP attach → FILL ring 填充 → poll 收包
#
# 前置条件：
#   - 01_build_app.sh 已成功编译
#   - 02_prepare_kernel_netdev.sh 已确认网卡可用
#   - （可选）00_check_env.sh 已确认工具链正常
#
# 使用：
#   sudo ./scripts/03_run_af_xdp_socket_smoke.sh
#
# 输出：
#   - AF_XDP_SOCKET_SMOKE_COMMAND.txt（实际执行的命令）
#   - AF_XDP_SOCKET_SMOKE.log（程序输出，含 PASS 标记）
#
# 通过标准：
#   - XSK_SOCKET_READY 出现
#   - XSKMAP_REGISTERED 出现
#   - 程序正常退出 bye
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 需要 root 权限（XDP attach 需要）
require_root

# 拒绝在管理网口上操作
refuse_management_iface "run AF_XDP socket smoke"

REC_DIR="$(latest_record_dir)"
OUT="${REC_DIR}/AF_XDP_SOCKET_SMOKE.log"
CMD_OUT="${REC_DIR}/AF_XDP_SOCKET_SMOKE_COMMAND.txt"

# 检查程序是否已编译
if [[ ! -x "${APP_DIR}/build/af_xdp_rings" || ! -f "${APP_DIR}/build/af_xdp_kern.bpf.o" ]]; then
    echo "ERROR: app not built. Run ./scripts/01_build_app.sh first." >&2
    exit 2
fi

{
    # 记录本次执行的完整命令（方便复现）
    write_env_header
    echo
    echo "sudo AF_XDP_MODE=${AF_XDP_MODE} AF_XDP_QUEUE=${AF_XDP_QUEUE} AF_XDP_BIND_MODE=${AF_XDP_BIND_MODE} AF_XDP_DURATION=${AF_XDP_DURATION} $0"
} > "${CMD_OUT}"

# 运行 AF_XDP 收包程序（内部调用 run_af_xdp_app）
run_af_xdp_app "${OUT}"