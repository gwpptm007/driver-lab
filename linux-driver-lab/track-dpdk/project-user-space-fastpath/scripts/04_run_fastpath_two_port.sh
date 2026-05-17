#!/usr/bin/env bash
# 脚本: 04_run_fastpath_two_port.sh
# 功能: 运行 fastpath-lite 双端口转发（需要两个物理网卡）
# 用法: sudo DPDK_PCI_1=0000:0c:00.0 ./scripts/04_run_fastpath_two_port.sh
#
# ========== 双端口与单端口的区别 ==========
# 03 单端口：只有一个 DPDK 网卡，包从同一个口进又从同一个口出（上下行同一链路）
# 04 双端口：有两个 DPDK 网卡，包从一个口进从另一个口出（真正转发）
# ==============================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"  # 加载公共函数库

# ========== 环境检查 ==========
require_root_for_write        # 检查 root 权限
guard_not_mgmt_pci            # 检查主 PCI 不能是管理口
require_app_bin               # 检查 APP_BIN 是否存在

# ========== 检查第二个 PCI 地址 ==========
# DPDK_PCI 是主网卡（已在环境变量或 common.sh 中定义）
# DPDK_PCI_1 是第二个网卡，需要通过环境变量传入
if [[ -z "${DPDK_PCI_1}" ]]; then
    echo "ERROR: DPDK_PCI_1 is empty. 需要指定第二个网卡的 PCI 地址" >&2
    echo "示例: sudo DPDK_PCI_1=0000:0c:00.0 $0" >&2
    exit 1
fi

# ========== 记录目录初始化 ==========
RECORD_DIR="$(ensure_record_dir)"           # 创建 records/YYYYMMDD_HHMMSS/
init_record_files "${RECORD_DIR}"            # 初始化记录文件
OUT="${RECORD_DIR}/FASTPATH_TWO_PORT.log"             # 执行日志
CMD_OUT="${RECORD_DIR}/FASTPATH_TWO_PORT_COMMAND.txt" # 命令记录
: > "${OUT}"
: > "${CMD_OUT}"

# ========== 构建命令 ==========
# base_app_args() 返回应用层参数
APP_ARGS=( $(base_app_args) )

# ========== 关键区别：两个 -a 参数指定两个物理网卡 ==========
# 03 单端口：只有一个 -a ${DPDK_PCI}
# 04 双端口：两个 -a，分别指定主网卡和第二个网卡
#
# 示例展开后：
#   ./app/build/fastpath-lite \
#     -l 0-1 -n 4 \
#     --file-prefix=fastpath_sp_two \    # 加 _two 后缀区分
#     -a 0000:0b:00.0 \                  # 主网卡
#     -a 0000:0c:00.0 \                  # 第二网卡
#     -- \
#     --run-seconds 20
CMD=("${APP_BIN}" -l "${FASTPATH_LCORES}" -n "${FASTPATH_MEMORY_CHANNELS}" --file-prefix "${FASTPATH_FILE_PREFIX}_two" -a "${DPDK_PCI}" -a "${DPDK_PCI_1}" -- "${APP_ARGS[@]}")

# ========== 记录命令 ==========
append_command_log "${RECORD_DIR}" "sudo" "${CMD[@]}"
printf '%q ' "${CMD[@]}" > "${CMD_OUT}"
echo >> "${CMD_OUT}"

# ========== 执行并记录输出 ==========
{
    echo "# FASTPATH_TWO_PORT"
    echo
    echo "## command"
    cat "${CMD_OUT}"
    echo
    # ========== 核心：执行命令 ==========
    "${CMD[@]}"
    echo "rc=$?"
} >> "${OUT}" 2>&1

echo "[OK] two-port run saved: ${OUT}"