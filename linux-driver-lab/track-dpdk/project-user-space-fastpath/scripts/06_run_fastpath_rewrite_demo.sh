#!/usr/bin/env bash
# 脚本: 06_run_fastpath_rewrite_demo.sh
# 功能: 运行 fastpath-lite rewrite 演示模式（使用虚拟 vdev 无需物理网卡）
# 用法: sudo ./scripts/06_run_fastpath_rewrite_demo.sh
#
# ========== rewrite 演示原理 ==========
# 本脚本使用 DPDK vdev null pair（虚拟设备对）来演示 rewrite 功能：
#   - 不需要物理网卡（vmxnet3）
#   - 不需要绑定 PCI
#   - 发送的包会被 null vdev 直接环回，形成闭环
#   - 适合在没有物理网卡的环境中测试 rewrite 代码路径
#
# rewrite 功能：对 UDP 包的 IP 地址和端口进行替换
#   原始包：src_ip=任意, dst_ip=任意, src_port=任意, dst_port=任意
#   rewrite后：src_ip=10.10.1.10, dst_ip=10.10.2.20, src_port=5000, dst_port=6000
# ==============================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"  # 加载公共函数库

# ========== 环境检查 ==========
require_root_for_write        # 检查 root 权限
require_app_bin               # 检查 APP_BIN 是否存在

# ========== 为什么用 vdev 而非物理网卡？ ==========
# 使用虚拟设备（null pair）进行测试的好处：
#   1. 不需要物理网卡（vmxnet3）
#   2. 不需要执行 02_prepare_vmxnet3.sh 绑定驱动
#   3. 不影响 SSH 管理口
#   4. 可以快速 smoke-test rewrite 代码路径
#
# 如果要测试物理网卡：
#   复制本脚本打印的命令，手动修改为 -a 0000:0b:00.0 格式
#   然后使用 03_run_fastpath_single_port.sh 或 04_run_fastpath_two_port.sh
# ========== 启用 rewrite 功能 ==========
# 设置环境变量，供 base_app_args() 函数读取
FASTPATH_UDP_ONLY=1              # 只允许 UDP 包通过（非 UDP 包丢弃）
FASTPATH_REWRITE_ENABLE=1         # 启用 rewrite 功能
# rewrite 参数：替换 src/dst 的 IP 地址和端口
# 这些参数会被 base_app_args() 读取并添加到应用参数中
FASTPATH_EXTRA_APP_ARGS="--rewrite-src-ip 10.10.1.10 \
                         --rewrite-dst-ip 10.10.2.20 \
                         --rewrite-src-port 5000 \
                         --rewrite-dst-port 6000 \
                         ${FASTPATH_EXTRA_APP_ARGS}"
export FASTPATH_UDP_ONLY FASTPATH_REWRITE_ENABLE FASTPATH_EXTRA_APP_ARGS

# ========== 记录目录初始化 ==========
RECORD_DIR="$(ensure_record_dir)"           # 创建 records/YYYYMMDD_HHMMSS/
init_record_files "${RECORD_DIR}"            # 初始化记录文件
OUT="${RECORD_DIR}/FASTPATH_REWRITE_DEMO.log"          # 执行日志
CMD_OUT="${RECORD_DIR}/FASTPATH_REWRITE_DEMO_COMMAND.txt"  # 命令记录
: > "${OUT}"
: > "${CMD_OUT}"

# ========== 构建命令 ==========
# base_app_args() 会读取上面 export 的环境变量，返回：
#   --udp-only 1 --rewrite 1 --rewrite-src-ip 10.10.1.10 ...
APP_ARGS=( $(base_app_args) )

# ========== 命令构建：使用 vdev null pair（不需要物理网卡）==========
# 关键区别于 03_run_fastpath_single_port.sh：
#   03: 使用 -a ${DPDK_PCI} 指定物理 PCI 设备
#   06: 使用 --no-pci --vdev 指定虚拟设备
#
# --no-pci：不扫描物理 PCI（避免需要真实网卡）
# --vdev ${FASTPATH_VDEV0}：虚拟设备 0（发送端）
# --vdev ${FASTPATH_VDEV1}：虚拟设备 1（接收端，环形回到发送端）
# --file-prefix：加 _rewrite 后缀，隔离普通 fastpath 实例的资源
#
# 示例展开后：
#   ./app/build/fastpath-lite \
#     -l 0-1 -n 4 \
#     --file-prefix=fastpath_sp_rewrite \   # 注意加了 _rewrite 后缀
#     --no-pci \
#     --vdev=net_vdev0,mac=00:00:00:00:00:01 \
#     --vdev=net_vdev1,mac=00:00:00:00:00:02 \
#     -- \
#     --udp-only 1 --rewrite 1 --rewrite-src-ip 10.10.1.10 ...
CMD=("${APP_BIN}" -l "${FASTPATH_LCORES}" -n "${FASTPATH_MEMORY_CHANNELS}" --file-prefix "${FASTPATH_FILE_PREFIX}_rewrite" --no-pci --vdev "${FASTPATH_VDEV0}" --vdev "${FASTPATH_VDEV1}" -- "${APP_ARGS[@]}")

# ========== 记录命令 ==========
append_command_log "${RECORD_DIR}" "sudo" "${CMD[@]}"
printf '%q ' "${CMD[@]}" > "${CMD_OUT}"
echo >> "${CMD_OUT}"

# ========== 执行并记录输出 ==========
{
    echo "# FASTPATH_REWRITE_DEMO"
    echo
    echo "## command"
    cat "${CMD_OUT}"
    echo
    # ========== 核心：执行命令 ==========
    # "${CMD[@]}" 是 bash 数组展开，保持参数结构完整
    "${CMD[@]}"
    echo "rc=$?"
} >> "${OUT}" 2>&1

echo "[OK] rewrite demo saved: ${OUT}"