#!/usr/bin/env bash
# 脚本: 02_run_virtio_user_vhost_pair.sh
# 功能: 同时启动 backend (vhost-user) 和 frontend (virtio-user) testpmd，验证本机互联闭环
# 用法: sudo ./scripts/02_run_virtio_user_vhost_pair.sh
#
# ========== 通信架构 ==========
# 本脚本同时运行两个 testpmd 实例：
#
# 1. backend (net_vhost0, rxonly 模式)
#    - DPDK vhost-user PMD，在 server 模式下创建 UDS socket
#    - 轮询接收来自 frontend 的数据包
#    - 与 frontend 对接后形成完整的 virtio 数据面
#
# 2. frontend (net_virtio_user0, txonly 模式)
#    - DPDK virtio-user PMD，作为 client 连接到 backend 的 socket
#    - 不断发送数据包给 backend
#
# 数据流: frontend (txonly) ──UDS socket──► backend (rxonly)
#         /tmp/dpdk-vhost-user0
#
# ========== 关键参数说明 ==========
# backend EAL:
#   -l 0-1              使用 CPU 0-1 核
#   -n 4                内存通道数
#   --file-prefix=vhost_backend  文件前缀（与 frontend 隔离）
#   --vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1,client=0
#                       vhost-user 设备参数
#   --no-pci            不扫描物理 PCI
#   --forward-mode=rxonly  只接收模式
#
# frontend EAL:
#   -l 2-3              使用 CPU 2-3 核（与 backend 隔离）
#   --file-prefix=virtio_frontend
#   --vdev=net_virtio_user0,path=/tmp/dpdk-vhost-user0,queues=1
#                       virtio-user 设备参数（连接到 backend 的 socket）
#   --forward-mode=txonly  只发送模式
#
# ==============================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 需要 root 权限（大页配置需要）
require_root_for_write
# 检查 socket 路径是否在安全目录
safe_socket_path_check

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

BACKEND_LOG="${RECORD_DIR}/TESTPMD_BACKEND.log"
FRONTEND_LOG="${RECORD_DIR}/TESTPMD_FRONTEND.log"
SOCK_RECORD="${RECORD_DIR}/VHOST_SOCKET.txt"
CMD_RECORD="${RECORD_DIR}/TESTPMD_COMMANDS.txt"
RUNTIME_RECORD="${RECORD_DIR}/RUNTIME_STATUS.txt"
BACKEND_FIFO="${RECORD_DIR}/backend-input.fifo"
FRONTEND_FIFO="${RECORD_DIR}/frontend-input.fifo"
BACKEND_RC_FILE="${RECORD_DIR}/BACKEND_RC.txt"
FRONTEND_RC_FILE="${RECORD_DIR}/FRONTEND_RC.txt"

: > "${BACKEND_LOG}"
: > "${FRONTEND_LOG}"
: > "${SOCK_RECORD}"
: > "${CMD_RECORD}"
: > "${RUNTIME_RECORD}"
rm -f "${BACKEND_FIFO}" "${FRONTEND_FIFO}"
mkfifo "${BACKEND_FIFO}" "${FRONTEND_FIFO}"

BACKEND_PID=""
FRONTEND_PID=""

# 清理函数：退出时杀死两个 testpmd 进程、关闭 FIFO
cleanup_pair() {
    set +e
    [[ -n "${BACKEND_PID}" ]] && kill "${BACKEND_PID}" 2>/dev/null || true
    [[ -n "${FRONTEND_PID}" ]] && kill "${FRONTEND_PID}" 2>/dev/null || true
    exec 3>&- 2>/dev/null || true
    exec 4>&- 2>/dev/null || true
    rm -f "${BACKEND_FIFO}" "${FRONTEND_FIFO}"
}
# EXIT trap：脚本退出时自动执行清理
trap cleanup_pair EXIT

if testpmd="$(find_testpmd)"; then
    :
else
    echo "ERROR: dpdk-testpmd/testpmd not found. Install DPDK tools or set TESTPMD_BIN." >&2
    exit 1
fi

if [[ -S "${VHOST_SOCKET}" ]]; then
    echo "[INFO] removing stale socket: ${VHOST_SOCKET}" | tee -a "${SOCK_RECORD}"
    rm -f "${VHOST_SOCKET}"
elif [[ -e "${VHOST_SOCKET}" ]]; then
    echo "ERROR: ${VHOST_SOCKET} exists but is not a UNIX socket; refuse to remove." >&2
    exit 2
fi

BACKEND_VDEV="$(backend_vdev_arg)"
FRONTEND_VDEV="$(frontend_vdev_arg)"

BACKEND_EAL=("-l" "${BACKEND_CORES}" "-n" "${TESTPMD_MEM_CHANNELS}" "--file-prefix=${BACKEND_FILE_PREFIX}" "--vdev=${BACKEND_VDEV}")
FRONTEND_EAL=("-l" "${FRONTEND_CORES}" "-n" "${TESTPMD_MEM_CHANNELS}" "--file-prefix=${FRONTEND_FILE_PREFIX}" "--vdev=${FRONTEND_VDEV}")
if [[ "${NO_PCI}" == "1" ]]; then
    BACKEND_EAL+=("--no-pci")
    FRONTEND_EAL+=("--no-pci")
fi
BACKEND_APP=("--port-topology=${BACKEND_PORT_TOPOLOGY}" "--forward-mode=${BACKEND_FORWARD_MODE}" "--auto-start" "--stats-period=${TESTPMD_STATS_PERIOD}")
FRONTEND_APP=("--port-topology=${FRONTEND_PORT_TOPOLOGY}" "--forward-mode=${FRONTEND_FORWARD_MODE}" "--auto-start" "--stats-period=${TESTPMD_STATS_PERIOD}")
if [[ -n "${BACKEND_APP_EXTRA}" ]]; then
    # shellcheck disable=SC2206
    EXTRA_ARR=( ${BACKEND_APP_EXTRA} )
    BACKEND_APP+=("${EXTRA_ARR[@]}")
fi
if [[ -n "${FRONTEND_APP_EXTRA}" ]]; then
    # shellcheck disable=SC2206
    EXTRA_ARR=( ${FRONTEND_APP_EXTRA} )
    FRONTEND_APP+=("${EXTRA_ARR[@]}")
fi

{
    echo "# TESTPMD_COMMANDS"
    echo
    echo "## backend"
    printf '%q ' "${testpmd}" "${BACKEND_EAL[@]}" -- "${BACKEND_APP[@]}"
    echo
    echo
    echo "## frontend"
    printf '%q ' "${testpmd}" "${FRONTEND_EAL[@]}" -- "${FRONTEND_APP[@]}"
    echo
    echo
    echo "## vdev"
    echo "BACKEND_VDEV=${BACKEND_VDEV}"
    echo "FRONTEND_VDEV=${FRONTEND_VDEV}"
} > "${CMD_RECORD}"

# 预先打开 FIFO（读写模式），避免 testpmd 提前退出时阻塞
exec 3<>"${BACKEND_FIFO}"
exec 4<>"${FRONTEND_FIFO}"

# ========== Step 1: 启动 backend (vhost-user) ==========
# backend 创建 UDS socket，frontend 稍后会连接它
(
    set +e
    # timeout 限制运行时长，<${BACKEND_FIFO} 从管道读取命令
    timeout "${BACKEND_RUNTIME}" "${testpmd}" "${BACKEND_EAL[@]}" -- "${BACKEND_APP[@]}" < "${BACKEND_FIFO}" > "${BACKEND_LOG}" 2>&1
    echo "$?" > "${BACKEND_RC_FILE}"
) &
BACKEND_PID=$!  # 记录 backend testpmd 的 PID

# 记录 backend 运行时状态
{
    echo "# RUNTIME_STATUS"
    echo "date=$(date '+%F %T')"
    echo "testpmd_bin=${testpmd}"
    echo "backend_pid=${BACKEND_PID}"
    echo "backend_vdev=${BACKEND_VDEV}"
    echo "frontend_vdev=${FRONTEND_VDEV}"
    echo "vhost_socket=${VHOST_SOCKET}"
    echo
} >> "${RUNTIME_RECORD}"

# ========== Step 2: 等待 backend 创建 UDS socket ==========
# 轮询检查 socket 是否被创建（最多 40 次 × 0.5 秒 = 20 秒超时）
# socket 由 backend 的 net_vhost0 驱动在启动时自动创建
SOCKET_READY=0
for _ in $(seq 1 40); do
    if [[ -S "${VHOST_SOCKET}" ]]; then
        SOCKET_READY=1
        break
    fi
    # 如果 backend 进程已退出，不再等待
    if ! pid_alive "${BACKEND_PID}"; then
        break
    fi
    sleep 0.5
done

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
} >> "${SOCK_RECORD}" 2>&1

# socket 未创建则报错退出
if [[ "${SOCKET_READY}" != "1" ]]; then
    echo "ERROR: backend did not create vhost-user socket. See ${BACKEND_LOG} and ${SOCK_RECORD}" >&2
    exit 3
fi

# ========== Step 3: 启动 frontend (virtio-user) ==========
# frontend 作为 client 连接到 backend 创建的 socket
(
    set +e
    timeout "${FRONTEND_RUNTIME}" "${testpmd}" "${FRONTEND_EAL[@]}" -- "${FRONTEND_APP[@]}" < "${FRONTEND_FIFO}" > "${FRONTEND_LOG}" 2>&1
    echo "$?" > "${FRONTEND_RC_FILE}"
) &
FRONTEND_PID=$!

# 记录 frontend 启动信息
{
    echo "frontend_pid=${FRONTEND_PID}"
    echo "frontend_start_time=$(date '+%F %T')"
    echo
} >> "${RUNTIME_RECORD}"

# ========== Step 4: 等待两端协商完成 ==========
# frontend 连接 socket 后，两端需要交换共享内存的文件描述符（FD）
# 这就是 vhost-user 协议的核心：通过 UDS 传递 FD，实现零拷贝
# PAIR_WARMUP_SECONDS = 6 秒，等待协商完成
sleep "${PAIR_WARMUP_SECONDS}"

# ========== Step 5: 向两端发送命令收集证据 ==========
# 通过 FIFO 向 backend (fd 3) 和 frontend (fd 4) 发送交互命令
# 顺序：先 frontend (quit 先发)，再 backend (quit 后发)
printf 'show port info all\n' >&3 || true
printf 'show port info all\n' >&4 || true
sleep 1
printf 'show port stats all\n' >&3 || true
printf 'show port stats all\n' >&4 || true
sleep 2
# 先停止 frontend（发送者），再停止 backend（接收者）
printf 'stop\n' >&4 || true
printf 'show port stats all\n' >&4 || true
printf 'quit\n' >&4 || true
sleep 1
printf 'stop\n' >&3 || true
printf 'show port stats all\n' >&3 || true
printf 'quit\n' >&3 || true

# 关闭 FIFO
exec 4>&-
exec 3>&-

set +e
wait "${FRONTEND_PID}"
FRONTEND_WAIT_RC=$?
wait "${BACKEND_PID}"
BACKEND_WAIT_RC=$?
set -e

rm -f "${BACKEND_FIFO}" "${FRONTEND_FIFO}"
trap - EXIT

BACKEND_RC="unknown"
FRONTEND_RC="unknown"
[[ -f "${BACKEND_RC_FILE}" ]] && BACKEND_RC="$(cat "${BACKEND_RC_FILE}" 2>/dev/null || echo unknown)"
[[ -f "${FRONTEND_RC_FILE}" ]] && FRONTEND_RC="$(cat "${FRONTEND_RC_FILE}" 2>/dev/null || echo unknown)"

{
    echo "## Final"
    echo "backend_wait_rc=${BACKEND_WAIT_RC}"
    echo "frontend_wait_rc=${FRONTEND_WAIT_RC}"
    echo "backend_testpmd_rc=${BACKEND_RC}"
    echo "frontend_testpmd_rc=${FRONTEND_RC}"
    echo "socket_exists_after_exit=$([[ -e "${VHOST_SOCKET}" ]] && echo yes || echo no)"
    echo
    echo "## Hugepage after pair"
    grep Huge /proc/meminfo || true
} >> "${RUNTIME_RECORD}" 2>&1

if [[ "${BACKEND_RC}" != "0" && "${BACKEND_RC}" != "124" ]]; then
    echo "ERROR: backend testpmd exited with rc=${BACKEND_RC}. See ${BACKEND_LOG}" >&2
    exit 4
fi
if [[ "${FRONTEND_RC}" != "0" && "${FRONTEND_RC}" != "124" ]]; then
    echo "ERROR: frontend testpmd exited with rc=${FRONTEND_RC}. See ${FRONTEND_LOG}" >&2
    exit 5
fi

cat <<EOF_OUT
[OK] virtio-user <-> vhost-user pair smoke finished.

Evidence:
  commands : ${CMD_RECORD}
  backend  : ${BACKEND_LOG}
  frontend : ${FRONTEND_LOG}
  socket   : ${SOCK_RECORD}
  runtime  : ${RUNTIME_RECORD}

Next:
  ./scripts/03_collect_stats.sh
EOF_OUT
