#!/usr/bin/env bash
set -euo pipefail

STAGE05_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT_DIR="$STAGE05_ROOT/output"
ENV_FILE="$STAGE05_ROOT/env/stage05_virtio_param.env"

stage05_import_env() {
    local line key val
    [[ -f "$ENV_FILE" ]] || return 0
    while IFS= read -r line; do
        case "$line" in
            ''|'#'*) continue ;;
        esac
        if [[ "$line" == *'?='* ]]; then
            key=${line%%\?=*}
            val=${line#*\?=}
            if [[ -z ${!key+x} ]]; then
                printf -v "$key" '%s' "$val"
                export "$key"
            fi
        elif [[ "$line" == *'='* ]]; then
            key=${line%%=*}
            val=${line#*=}
            printf -v "$key" '%s' "$val"
            export "$key"
        fi
    done < "$ENV_FILE"
}

stage05_import_env

stage05_find_virtio_net_source() {
    local candidates=()
    [[ -n "${VIRTIO_NET_SOURCE:-}" ]] && candidates+=("$VIRTIO_NET_SOURCE")
    [[ -n "${KERNEL_SOURCE_ROOT:-}" ]] && candidates+=("$KERNEL_SOURCE_ROOT/drivers/net/virtio_net.c")
    candidates+=(
        "/lib/modules/$(uname -r)/source/drivers/net/virtio_net.c"
        "/usr/src/linux-headers-$(uname -r)/drivers/net/virtio_net.c"
    )
    local c
    for c in "${candidates[@]}"; do
        [[ -f "$c" ]] && { printf '%s
' "$c"; return 0; }
    done
    if [[ -n "${KERNEL_SOURCE_ROOT:-}" && -d "${KERNEL_SOURCE_ROOT:-}" ]]; then
        c=$(find "$KERNEL_SOURCE_ROOT" -path '*/drivers/net/virtio_net.c' 2>/dev/null | head -n 1 || true)
        [[ -n "$c" && -f "$c" ]] && { printf '%s
' "$c"; return 0; }
    fi
    return 1
}

stage05_resolve_cross_compile() {
    local arch="${TARGET_ARCH:-host}"
    [[ -n "${CROSS_COMPILE:-}" ]] && { printf '%s
' "$CROSS_COMPILE"; return 0; }
    case "$arch" in
        arm64) printf 'aarch64-linux-gnu-
' ;;
        *) printf '
' ;;
    esac
}

stage05_resolve_qemu_bin() {
    local mode="${RUN_MODE:-host}"
    [[ -n "${QEMU_BIN:-}" ]] && { printf '%s
' "$QEMU_BIN"; return 0; }
    case "$mode" in
        qemu-x86_64) printf '%s
' "${QEMU_SYSTEM_X86_64:-qemu-system-x86_64}" ;;
        qemu-arm64) printf '%s
' "${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}" ;;
        *) printf '
' ;;
    esac
}

stage05_has_cmd() {
    command -v "$1" >/dev/null 2>&1 && echo yes || echo no
}
