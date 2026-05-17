#!/usr/bin/env bash
# 脚本: 02_run_vhost_testpmd.sh
# 功能: 使用 dpdk-testpmd 启动 DPDK vhost-user backend（UNIX socket 模式）
# 用法: sudo ./scripts/02_run_vhost_testpmd.sh
#
# ========== vhost-user 原理 ==========
# vhost-user 是 DPDK 实现的 virtio backend，通过 UNIX domain socket 与 QEMU 通信。
# 本脚本不依赖物理网卡，使用 --vdev=net_vhost0 创建虚拟 vhost-user 设备。
#
# ========== UDS socket 创建说明 ==========
# socket 由 DPDK net_vhost0 驱动在 testpmd 启动时自动创建，不是脚本手动创建的。
# 脚本负责：
#   1. 清理旧残留 socket（rm -f ${VHOST_SOCKET}）
#   2. 通过 --vdev=net_vhost0,iface=${VHOST_SOCKET} 告诉 DPDK 在哪个路径创建
#   3. 轮询等待 socket 文件出现（-S 检查是否为 socket）
#
# socket 文件特征（不是普通文件）：
#   ls -l /tmp/dpdk-vhost-user0  → 显示 "srwxr-xr-x"（开头's'=Socket）
#   file /tmp/dpdk-vhost-user0   → 显示 "Unix domain socket"
#   ss -xl | grep vhost          → 显示 LISTEN 状态
# ==========================================
#
# 关键参数：
#   --vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1
#     创建名为 net_vhost0 的虚拟设备，UNIX socket 路径为 /tmp/dpdk-vhost-user0
#     queues=1 表示使用 1 个 virtqueue（Rx/Tx 各一个）
#
#   --no-pci
#     不扫描/绑定任何物理 PCI 设备，本实验只使用纯内存/软件模拟
#
# ==============================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 需要 root 权限（大页和运行时目录需要）
require_root_for_write
# 检查 socket 路径是否在安全目录（/tmp, /run, /var/run）
safe_socket_path_check

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

# 记录输出文件路径
LOG="${RECORD_DIR}/TESTPMD_VHOST.log"                    # testpmd 运行日志
SOCK_RECORD="${RECORD_DIR}/VHOST_SOCKET.txt"            # socket 创建状态
CMD_RECORD="${RECORD_DIR}/TESTPMD_COMMAND.txt"          # 实际执行的命令
RUNTIME_RECORD="${RECORD_DIR}/RUNTIME_STATUS.txt"       # 运行时状态
FIFO="${RECORD_DIR}/testpmd-input.fifo"                 # 向 testpmd 发送命令的管道
RC_FILE="${RECORD_DIR}/TESTPMD_RC.txt"                  # testpmd 退出码

: > "${LOG}"
: > "${SOCK_RECORD}"
: > "${RUNTIME_RECORD}"
# 创建命名管道（FIFO），用于向 testpmd 交互式发送命令（如 show port info）
rm -f "${FIFO}"
mkfifo "${FIFO}"

# 查找 dpdk-testpmd 可执行文件
if testpmd="$(find_testpmd)"; then
    :
else
    echo "ERROR: dpdk-testpmd/testpmd not found. Install DPDK tools or set TESTPMD_BIN." >&2
    exit 1
fi

# 清理上次运行遗留的 socket（如果存在且不是 socket 文件则报错拒绝删除）
# 注意：socket 不是脚本创建的，而是 DPDK 的 net_vhost0 驱动在 testpmd 启动时自动创建
if [[ -S "${VHOST_SOCKET}" ]]; then
    echo "[INFO] removing stale socket: ${VHOST_SOCKET}" | tee -a "${SOCK_RECORD}"
    rm -f "${VHOST_SOCKET}"
elif [[ -e "${VHOST_SOCKET}" ]]; then
    echo "ERROR: ${VHOST_SOCKET} exists but is not a UNIX socket; refuse to remove." >&2
    exit 2
fi

# ========== UDS socket 创建原理 ==========
# socket 由 DPDK net_vhost0 驱动在 testpmd 启动时自动创建，脚本只是：
#   1. 清理旧残留 socket（rm -f）
#   2. 通过 --vdev=net_vhost0,iface=${VHOST_SOCKET} 告诉 DPDK 在哪个路径创建
#   3. 轮询等待 socket 文件出现（验证创建成功）
#
# socket 文件类型为 Unix Domain Socket，不是普通文件：
#   ls -l /tmp/dpdk-vhost-user0  → 显示 "srwxr-xr-x"（s=Socket）
#   file /tmp/dpdk-vhost-user0   → 显示 "Unix domain socket"
# ==========================================

# 生成 vhost-user 虚拟设备参数
VDEV_ARG="$(vhost_vdev_arg)"
# 构建 EAL 参数：CPU 核、内存通道数、文件前缀、vhost 设备、禁用 PCI 扫描
EAL_ARGS=("-l" "${TESTPMD_CORES}" "-n" "${TESTPMD_MEM_CHANNELS}" "--file-prefix=${TESTPMD_FILE_PREFIX}" "--vdev=${VDEV_ARG}")
if [[ "${NO_PCI}" == "1" ]]; then
    EAL_ARGS+=("--no-pci")  # 不扫描物理网卡，本实验使用纯软件模拟
fi
# 构建 testpmd 应用参数：链式拓扑、转发模式、自动启动、统计周期
APP_ARGS=("--port-topology=chained" "--forward-mode=${TESTPMD_FORWARD_MODE}" "--auto-start" "--stats-period=${TESTPMD_STATS_PERIOD}")

# 记录实际执行的命令
{
    echo "# TESTPMD_COMMAND"
    echo
    printf '%q ' "${testpmd}" "${EAL_ARGS[@]}" -- "${APP_ARGS[@]}"
    echo
    echo
    echo "VDEV_ARG=${VDEV_ARG}"
} > "${CMD_RECORD}"

# 后台运行 testpmd，通过 FIFO 向其发送命令
# 注意：testpmd 需要在后台运行，这样才能通过 FIFO 交互
(
    set +e
    # timeout 限制运行时长，<${FIFO} 从管道读取命令，>${LOG} 输出重定向到日志
    timeout "${TESTPMD_RUNTIME}" "${testpmd}" "${EAL_ARGS[@]}" -- "${APP_ARGS[@]}" < "${FIFO}" > "${LOG}" 2>&1
    echo "$?" > "${RC_FILE}"
) &
TPMD_PID=$!  # 记录 testpmd 的进程 ID

# 打开 FIFO 供写入（fd 3），在 quit/timeout 之前保持打开
exec 3>"${FIFO}"

# 记录运行时状态
{
    echo "# RUNTIME_STATUS"
    echo "date=$(date '+%F %T')"
    echo "testpmd_pid=${TPMD_PID}"
    echo "testpmd_bin=${testpmd}"
    echo "vhost_socket=${VHOST_SOCKET}"
    echo
} >> "${RUNTIME_RECORD}"

# 等待 socket 创建完成（轮询最多 30 次，每次 0.5 秒，共 15 秒超时）
# 本实验不需要 virtio peer 连接，只验证 socket 能被 DPDK 创建
# -S 检查文件是否存在且为 socket（区别于普通文件或目录）
SOCKET_READY=0
for _ in $(seq 1 30); do
    if [[ -S "${VHOST_SOCKET}" ]]; then
        SOCKET_READY=1
        break
    fi
    # 如果 testpmd 进程已退出（可能是出错），不再等待
    if ! kill -0 "${TPMD_PID}" 2>/dev/null; then
        break
    fi
    sleep 0.5
done

# 记录 socket 状态
{
    echo "# VHOST_SOCKET"
    echo "date=$(date '+%F %T')"
    echo "socket_ready=${SOCKET_READY}"
    echo
    if [[ -e "${VHOST_SOCKET}" ]]; then
        ls -l "${VHOST_SOCKET}" || true
        file "${VHOST_SOCKET}" 2>/dev/null || true
    else
        echo "${VHOST_SOCKET}: not exists"
    fi
    echo
    echo "## ss -xl"
    ss -xl 2>/dev/null | grep -E "$(basename "${VHOST_SOCKET}")|vhost|dpdk" || true
    echo
    echo "## lsof socket owner, if lsof exists"
    if command -v lsof >/dev/null 2>&1; then
        lsof -U "${VHOST_SOCKET}" 2>/dev/null || true
    else
        echo "lsof: not installed"
    fi
} >> "${SOCK_RECORD}" 2>&1

# 向 testpmd 发送命令：显示端口信息、统计、停止、退出
printf 'show port info all\n' >&3 || true
sleep 1
printf 'show port stats all\n' >&3 || true
sleep 2
printf 'stop\n' >&3 || true
sleep 1
printf 'quit\n' >&3 || true
exec 3>&-  # 关闭 FIFO

# 等待 testpmd 进程结束
set +e
wait "${TPMD_PID}"
WAIT_RC=$?
set -e
rm -f "${FIFO}"

# 读取 testpmd 退出码
RC="unknown"
if [[ -f "${RC_FILE}" ]]; then
    RC="$(cat "${RC_FILE}" 2>/dev/null || echo unknown)"
fi

# 记录最终状态
{
    echo
    echo "## Final"
    echo "wait_rc=${WAIT_RC}"
    echo "testpmd_rc=${RC}"
    echo "socket_exists_after_exit=$([[ -e "${VHOST_SOCKET}" ]] && echo yes || echo no)"
    echo
    echo "## Hugepage after testpmd"
    grep Huge /proc/meminfo || true
} >> "${RUNTIME_RECORD}" 2>&1

# socket 未创建则报错
if [[ "${SOCKET_READY}" != "1" ]]; then
    echo "ERROR: vhost-user socket was not created. See ${LOG} and ${SOCK_RECORD}" >&2
    exit 3
fi

# 非零且非 timeout 退出码（124）报错
if [[ "${RC}" != "0" && "${RC}" != "124" ]]; then
    echo "ERROR: testpmd exited with rc=${RC}. See ${LOG}" >&2
    exit 4
fi

cat <<EOF_OUT
[OK] vhost-user testpmd smoke finished.

Evidence:
  command : ${CMD_RECORD}
  log     : ${LOG}
  socket  : ${SOCK_RECORD}
  runtime : ${RUNTIME_RECORD}

Next:
  ./scripts/03_collect_stats.sh
EOF_OUT
