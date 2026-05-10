#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
APP_DIR="${LAB_DIR}/app"
RECORDS_DIR="${LAB_DIR}/records"
LAB_NAME="xdp-redirect-basics"

: "${AF_XDP_IFACE:=ens192}"
: "${AF_XDP_MANAGEMENT_IFACE:=ens33}"
: "${AF_XDP_PCI:=0000:0b:00.0}"
: "${AF_XDP_MODE:=skb}"
: "${AF_XDP_DURATION:=10}"
: "${AF_XDP_INTERVAL:=1}"
: "${AF_XDP_DRIVER:=vmxnet3}"

make_record_dir() {
    local ts
    ts="$(date +%Y%m%d-%H%M%S)"
    local dir="${RECORDS_DIR}/${ts}-${LAB_NAME}"
    mkdir -p "${dir}"
    echo "${dir}"
}

latest_record_dir() {
    local latest
    latest="$(find "${RECORDS_DIR}" -maxdepth 1 -type d -name "*-${LAB_NAME}" | sort | tail -1 || true)"
    if [[ -z "${latest}" ]]; then
        latest="$(make_record_dir)"
    fi
    echo "${latest}"
}

require_root() {
    if [[ "${EUID}" -ne 0 ]]; then
        echo "ERROR: this script requires root. Please run with sudo." >&2
        exit 1
    fi
}

refuse_management_iface() {
    local op="$1"
    if [[ "${AF_XDP_IFACE}" == "${AF_XDP_MANAGEMENT_IFACE}" ]]; then
        echo "ERROR: refusing to ${op} on management iface ${AF_XDP_MANAGEMENT_IFACE}" >&2
        echo "Set AF_XDP_IFACE to a non-management test interface." >&2
        exit 1
    fi
}

run_loader() {
    local action="$1"
    local duration="$2"
    local out="$3"
    mkdir -p "$(dirname "${out}")"
    (
        cd "${APP_DIR}/build"
        echo "COMMAND: ./xdp_loader run --ifname ${AF_XDP_IFACE} --mode ${AF_XDP_MODE} --action ${action} --duration ${duration} --interval ${AF_XDP_INTERVAL} --obj ./xdp_redirect_basics.bpf.o"
        ./xdp_loader run \
            --ifname "${AF_XDP_IFACE}" \
            --mode "${AF_XDP_MODE}" \
            --action "${action}" \
            --duration "${duration}" \
            --interval "${AF_XDP_INTERVAL}" \
            --obj ./xdp_redirect_basics.bpf.o
    ) 2>&1 | tee "${out}"
}

write_env_header() {
    cat <<EOF2
LAB=${LAB_NAME}
DATE=$(date -Is)
HOST=$(hostname)
KERNEL=$(uname -r)
AF_XDP_IFACE=${AF_XDP_IFACE}
AF_XDP_MANAGEMENT_IFACE=${AF_XDP_MANAGEMENT_IFACE}
AF_XDP_PCI=${AF_XDP_PCI}
AF_XDP_MODE=${AF_XDP_MODE}
AF_XDP_DURATION=${AF_XDP_DURATION}
AF_XDP_INTERVAL=${AF_XDP_INTERVAL}
EOF2
}
