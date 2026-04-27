#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root_for_write
safe_socket_path_check

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

LOG="${RECORD_DIR}/TESTPMD_VHOST.log"
SOCK_RECORD="${RECORD_DIR}/VHOST_SOCKET.txt"
CMD_RECORD="${RECORD_DIR}/TESTPMD_COMMAND.txt"
RUNTIME_RECORD="${RECORD_DIR}/RUNTIME_STATUS.txt"
FIFO="${RECORD_DIR}/testpmd-input.fifo"
RC_FILE="${RECORD_DIR}/TESTPMD_RC.txt"

: > "${LOG}"
: > "${SOCK_RECORD}"
: > "${RUNTIME_RECORD}"
rm -f "${FIFO}"
mkfifo "${FIFO}"

if testpmd="$(find_testpmd)"; then
    :
else
    echo "ERROR: dpdk-testpmd/testpmd not found. Install DPDK tools or set TESTPMD_BIN." >&2
    exit 1
fi

# Avoid stale socket from previous interrupted runs.
if [[ -S "${VHOST_SOCKET}" ]]; then
    echo "[INFO] removing stale socket: ${VHOST_SOCKET}" | tee -a "${SOCK_RECORD}"
    rm -f "${VHOST_SOCKET}"
elif [[ -e "${VHOST_SOCKET}" ]]; then
    echo "ERROR: ${VHOST_SOCKET} exists but is not a UNIX socket; refuse to remove." >&2
    exit 2
fi

VDEV_ARG="$(vhost_vdev_arg)"
EAL_ARGS=("-l" "${TESTPMD_CORES}" "-n" "${TESTPMD_MEM_CHANNELS}" "--file-prefix=${TESTPMD_FILE_PREFIX}" "--vdev=${VDEV_ARG}")
if [[ "${NO_PCI}" == "1" ]]; then
    EAL_ARGS+=("--no-pci")
fi
APP_ARGS=("--port-topology=chained" "--forward-mode=${TESTPMD_FORWARD_MODE}" "--auto-start" "--stats-period=${TESTPMD_STATS_PERIOD}")

{
    echo "# TESTPMD_COMMAND"
    echo
    printf '%q ' "${testpmd}" "${EAL_ARGS[@]}" -- "${APP_ARGS[@]}"
    echo
    echo
    echo "VDEV_ARG=${VDEV_ARG}"
} > "${CMD_RECORD}"

(
    set +e
    timeout "${TESTPMD_RUNTIME}" "${testpmd}" "${EAL_ARGS[@]}" -- "${APP_ARGS[@]}" < "${FIFO}" > "${LOG}" 2>&1
    echo "$?" > "${RC_FILE}"
) &
TPMD_PID=$!

# Open FIFO after the reader has been spawned. Keep fd 3 open until quit/timeout.
exec 3>"${FIFO}"

{
    echo "# RUNTIME_STATUS"
    echo "date=$(date '+%F %T')"
    echo "testpmd_pid=${TPMD_PID}"
    echo "testpmd_bin=${testpmd}"
    echo "vhost_socket=${VHOST_SOCKET}"
    echo
} >> "${RUNTIME_RECORD}"

# Wait for socket creation. No virtio peer is expected in this lab.
SOCKET_READY=0
for _ in $(seq 1 30); do
    if [[ -S "${VHOST_SOCKET}" ]]; then
        SOCKET_READY=1
        break
    fi
    if ! kill -0 "${TPMD_PID}" 2>/dev/null; then
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
    echo
    echo "## lsof socket owner, if lsof exists"
    if command -v lsof >/dev/null 2>&1; then
        lsof -U "${VHOST_SOCKET}" 2>/dev/null || true
    else
        echo "lsof: not installed"
    fi
} >> "${SOCK_RECORD}" 2>&1

# Ask testpmd to print evidence, then exit cleanly.
printf 'show port info all\n' >&3 || true
sleep 1
printf 'show port stats all\n' >&3 || true
sleep 2
printf 'stop\n' >&3 || true
sleep 1
printf 'quit\n' >&3 || true
exec 3>&-

set +e
wait "${TPMD_PID}"
WAIT_RC=$?
set -e
rm -f "${FIFO}"

RC="unknown"
if [[ -f "${RC_FILE}" ]]; then
    RC="$(cat "${RC_FILE}" 2>/dev/null || echo unknown)"
fi

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

if [[ "${SOCKET_READY}" != "1" ]]; then
    echo "ERROR: vhost-user socket was not created. See ${LOG} and ${SOCK_RECORD}" >&2
    exit 3
fi

# timeout rc=124 is acceptable if testpmd did not consume quit quickly, but normal clean quit is preferred.
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
