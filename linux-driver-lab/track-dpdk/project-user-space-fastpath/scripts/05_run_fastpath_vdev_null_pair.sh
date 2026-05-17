#!/usr/bin/env bash
# 脚本: 05_run_fastpath_vdev_null_pair.sh
# 功能: 运行 fastpath-lite 使用 vdev null pair 虚拟设备（不需要物理网卡）
# 用法: sudo ./scripts/05_run_fastpath_vdev_null_pair.sh
#
# ========== vdev null pair 原理 ==========
# null pair 是 DPDK 虚拟设备，由两个 vdev 组成成对工作：
#   net_vdev0（发送端）←→ net_vdev1（接收端）
#
# 数据流：
#   发送：net_vdev0 发出 → net_vdev1 接收
#   接收：net_vdev1 收到包后 → 环回到 net_vdev0 的发送队列
#   结果：包在两个虚拟设备之间闭环传输
#
# 用途：
#   - 不需要物理网卡，快速测试数据面逻辑
#   - 不影响 SSH 管理口
#   - 适合 smoke test 和开发调试
# ==============================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"  # 加载公共函数库

# ========== 环境检查 ==========
require_root_for_write        # 检查 root 权限
require_app_bin               # 检查 APP_BIN 是否存在

# ========== 记录目录初始化 ==========
RECORD_DIR="$(ensure_record_dir)"              # 创建 records/YYYYMMDD_HHMMSS/
init_record_files "${RECORD_DIR}"               # 初始化记录文件
OUT="${RECORD_DIR}/FASTPATH_VDEV_NULL_PAIR.log"            # 执行日志
CMD_OUT="${RECORD_DIR}/FASTPATH_VDEV_NULL_PAIR_COMMAND.txt" # 命令记录
: > "${OUT}"
: > "${CMD_OUT}"

# ========== 构建命令 ==========
# base_app_args() 返回应用层参数
APP_ARGS=( $(base_app_args) )

# ========== 命令构建：使用 vdev null pair（与 06 rewrite 相同）==========
# 与 06 的区别：
#   06 rewrite：启用 rewrite 功能，设置 rewrite 参数
#   05 vdev null pair：不做 rewrite，只是基本的收发测试
#
# --no-pci：不扫描物理 PCI（不需要真实网卡）
# --vdev ${FASTPATH_VDEV0}：虚拟设备 0（发送端）
# --vdev ${FASTPATH_VDEV1}：虚拟设备 1（接收端，环回）
# --file-prefix：加 _null 后缀，隔离资源
#
# 示例展开后：
#   ./app/build/fastpath-lite \
#     -l 0-1 -n 4 \
#     --file-prefix=fastpath_sp_null \
#     --no-pci \
#     --vdev=net_vdev0,mac=00:00:00:00:00:01 \
#     --vdev=net_vdev1,mac=00:00:00:00:00:02 \
#     -- \
#     --run-seconds 20
CMD=("${APP_BIN}" -l "${FASTPATH_LCORES}" -n "${FASTPATH_MEMORY_CHANNELS}" --file-prefix "${FASTPATH_FILE_PREFIX}_null" --no-pci --vdev "${FASTPATH_VDEV0}" --vdev "${FASTPATH_VDEV1}" -- "${APP_ARGS[@]}")

# ========== 记录命令 ==========
append_command_log "${RECORD_DIR}" "sudo" "${CMD[@]}"
printf '%q ' "${CMD[@]}" > "${CMD_OUT}"
echo >> "${CMD_OUT}"

# ========== 执行并记录输出 ==========
{
    echo "# FASTPATH_VDEV_NULL_PAIR"
    echo
    echo "## command"
    cat "${CMD_OUT}"
    echo
    # ========== 核心：执行命令 ==========
    "${CMD[@]}"
    echo "rc=$?"
} >> "${OUT}" 2>&1

echo "[OK] vdev null pair run saved: ${OUT}"