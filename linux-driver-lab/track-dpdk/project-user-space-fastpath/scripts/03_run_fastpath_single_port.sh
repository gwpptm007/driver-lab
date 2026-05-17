#!/usr/bin/env bash
# 脚本: 03_run_fastpath_single_port.sh
# 功能: 运行 fastpath-lite 单端口转发应用
# 用法: sudo ./scripts/03_run_fastpath_single_port.sh
#
# ========== 执行流程 ==========
# 1. 检查环境（root 权限、PCI 安全、APP_BIN 存在）
# 2. 构建命令：EAL 参数 + PCI 设备 + 应用参数
# 3. 记录命令到日志文件
# 4. 执行 fastpath-lite
# ===============================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"  # 加载公共函数库

# ========== 环境检查 ==========
require_root_for_write        # 检查 root 权限（DPDK 需要）
guard_not_mgmt_pci            # 检查 PCI 不能是管理口（防 SSH 断开）
require_app_bin               # 检查 APP_BIN 是否存在（编译产物）

# ========== 记录目录初始化 ==========
RECORD_DIR="$(ensure_record_dir)"           # 创建 records/YYYYMMDD_HHMMSS/
init_record_files "${RECORD_DIR}"            # 初始化 COMMANDS.md 等记录文件
OUT="${RECORD_DIR}/FASTPATH_SINGLE_PORT.log"       # 执行日志
CMD_OUT="${RECORD_DIR}/FASTPATH_SINGLE_PORT_COMMAND.txt"  # 纯命令记录
: > "${OUT}"                                # 清空日志
: > "${CMD_OUT}"

# ========== 构建命令 ==========
# base_app_args() 返回应用层参数（如 --udp-only, --rewrite 等），定义在 common.sh
APP_ARGS=( $(base_app_args) )

# CMD 是完整命令数组：EAL 参数 + PCI 设备 + 应用参数
# EAL 参数：-l（CPU核心）、-n（内存通道）、--file-prefix（隔离多实例）、-a（PCI设备）
# 示例展开后：
#   ./app/build/fastpath-lite -l 0-1 -n 4 --file-prefix=fastpath_sp -a 0000:0b:00.0 -- --udp-only 1 --run-seconds 20
CMD=("${APP_BIN}" -l "${FASTPATH_LCORES}" -n "${FASTPATH_MEMORY_CHANNELS}" --file-prefix "${FASTPATH_FILE_PREFIX}" -a "${DPDK_PCI}" -- "${APP_ARGS[@]}")

# ========== 记录命令 ==========
# append_command_log：记录到 COMMANDS.md（包含完整调用链）
append_command_log "${RECORD_DIR}" "sudo" "${CMD[@]}"
# printf '%q '：将命令数组格式化为可重放的字符串，写入 CMD_OUT 文件
printf '%q ' "${CMD[@]}" > "${CMD_OUT}"
echo >> "${CMD_OUT}"

# ========== 执行并记录输出 ==========
{
    echo "# FASTPATH_SINGLE_PORT"
    echo
    echo "## command"
    cat "${CMD_OUT}"           # 显示执行的命令
    echo
    echo "## hint"
    echo "${TRAFFIC_HINT}"     # 显示流量提示（如如何打流验证）
    echo
    # ========== 核心：执行命令 ==========
    # "${CMD[@]}" 是 bash 数组展开语法，原封不动执行数组所有元素
    # 为什么要用数组而不是字符串？
    #   $CMD        → 只取第一个元素（丢失参数）
    #   ${CMD[*]}   → 参数带空格会裂开（"My Documents" → My Documents）
    #   "${CMD[@]}" → 保持参数结构完整，参数有空格也能正确处理 ✅
    "${CMD[@]}"
    echo "rc=$?"              # 记录退出码
} >> "${OUT}" 2>&1

echo "[OK] single-port run saved: ${OUT}"
