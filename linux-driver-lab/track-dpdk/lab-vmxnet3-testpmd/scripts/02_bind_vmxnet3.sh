#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

ACTION="${1:-status}"
RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

guard_not_mgmt_pci

devbind="$(find_devbind || true)"
if [[ -z "${devbind}" ]]; then
    echo "ERROR: dpdk-devbind.py not found. Install dpdk tools or set DPDK_DEVBIND=/path/to/dpdk-devbind.py" >&2
    exit 1
fi

status() {
    "${devbind}" --status
    echo
    echo "Target:"
    echo "  DPDK_IF=${DPDK_IF}"
    echo "  DPDK_PCI=${DPDK_PCI}"
    echo "  DPDK_DRIVER=${DPDK_DRIVER}"
    echo "  MGMT_IF=${MGMT_IF}"
}

case "${ACTION}" in
    status)
        OUT="${RECORD_DIR}/BIND_STATUS.txt"
        {
            echo "# BIND_STATUS"
            echo "date=$(date '+%F %T')"
            echo
            status
        } > "${OUT}" 2>&1
        cat "${OUT}"
        echo
        echo "[OK] Bind status saved: ${OUT}"
        ;;

    bind)
        require_root_for_write
        if [[ "${DPDK_CONFIRM_BIND:-}" != "YES" ]]; then
            cat >&2 <<EOF
ERROR: bind will detach ${DPDK_IF}/${DPDK_PCI} from Linux kernel networking.
Please rerun explicitly:

  sudo DPDK_CONFIRM_BIND=YES $0 bind
EOF
            exit 2
        fi

        OUT_BEFORE="${RECORD_DIR}/BIND_BEFORE.txt"
        OUT_AFTER="${RECORD_DIR}/BIND_AFTER.txt"

        {
            echo "# BIND_BEFORE"
            echo "date=$(date '+%F %T')"
            echo
            status
            echo
            ip -br addr || true
            ethtool -i "${DPDK_IF}" || true
        } > "${OUT_BEFORE}" 2>&1

        modprobe vfio || true
        modprobe vfio-pci || true

        if ip link show "${DPDK_IF}" >/dev/null 2>&1; then
            ip link set "${DPDK_IF}" down || true
        fi

        "${devbind}" -b "${DPDK_DRIVER}" "${DPDK_PCI}" >> "${OUT_AFTER}" 2>&1 || {
            rc=$?
            echo "ERROR: dpdk-devbind failed, rc=${rc}. See ${OUT_AFTER}" >&2
            exit "${rc}"
        }

        {
            echo
            echo "# BIND_AFTER"
            echo "date=$(date '+%F %T')"
            echo
            status
        } >> "${OUT_AFTER}" 2>&1

        echo "[OK] Bind finished:"
        echo "  before: ${OUT_BEFORE}"
        echo "  after : ${OUT_AFTER}"
        ;;

    unbind|restore)
        require_root_for_write
        if [[ "${DPDK_CONFIRM_BIND:-}" != "YES" ]]; then
            cat >&2 <<EOF
ERROR: restore will rebind ${DPDK_PCI} to kernel vmxnet3.
Please rerun explicitly:

  sudo DPDK_CONFIRM_BIND=YES $0 unbind
EOF
            exit 2
        fi

        OUT="${RECORD_DIR}/UNBIND_TO_KERNEL.txt"
        : > "${OUT}"

        {
            echo "# UNBIND_TO_KERNEL"
            echo "date=$(date '+%F %T')"
            echo
            echo "## Before"
            status
            echo
        } >> "${OUT}" 2>&1

        modprobe vmxnet3 || true
        "${devbind}" -b vmxnet3 "${DPDK_PCI}" >> "${OUT}" 2>&1 || true

        {
            echo
            echo "## After"
            status
            echo
            ip link set "${DPDK_IF}" up 2>/dev/null || true
            ip -br addr || true
            ethtool -i "${DPDK_IF}" 2>/dev/null || true
        } >> "${OUT}" 2>&1

        echo "[OK] Restore attempt saved: ${OUT}"
        ;;

    *)
        cat >&2 <<EOF
Usage:
  $0 status
  sudo DPDK_CONFIRM_BIND=YES $0 bind
  sudo DPDK_CONFIRM_BIND=YES $0 unbind

Environment:
  DPDK_IF=${DPDK_IF}
  DPDK_PCI=${DPDK_PCI}
  DPDK_DRIVER=${DPDK_DRIVER}
  MGMT_IF=${MGMT_IF}
EOF
        exit 1
        ;;
esac
