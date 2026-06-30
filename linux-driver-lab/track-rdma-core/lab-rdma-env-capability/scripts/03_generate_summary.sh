#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="$(latest_record_dir)"
if [[ -z "${record_dir}" ]]; then
    echo "No record directory found." >&2
    exit 1
fi

summary="${record_dir}/SUMMARY.md"
env_log="${record_dir}/ENV_CHECK.log"
cap_log="${record_dir}/RDMA_CAPABILITY.log"
rxe_log="${record_dir}/SOFT_ROCE_BOUNDARY.log"

has_cmd_present() {
    local cmd="$1"
    grep -q "CMD_PRESENT ${cmd} " "${cap_log}" 2>/dev/null || grep -q "CMD_PRESENT ${cmd} " "${env_log}" 2>/dev/null
}

has_ibv_device() {
    awk '
        /^===== ibv_devices =====/ {in_section=1; next}
        /^===== / {in_section=0}
        in_section {
            line=$0
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", line)
            if (line == "" || line ~ /^[$]/ || line ~ /^device[[:space:]]+/ || line ~ /^[-]+/) next
            if (line ~ /^[^[:space:]]+[[:space:]]+[0-9a-fA-F:]+/) found=1
        }
        END {exit found ? 0 : 1}
    ' "${cap_log}" 2>/dev/null
}

has_rdma_dev() {
    awk '
        /^===== rdma dev =====/ {in_section=1; next}
        /^===== / {in_section=0}
        in_section && $0 ~ /^[[:space:]]*[0-9]+:/ {found=1}
        END {exit found ? 0 : 1}
    ' "${cap_log}" 2>/dev/null
}

rxe_available() {
    grep -q '^filename:.*rdma_rxe' "${rxe_log}" 2>/dev/null || grep -q '^filename:.*rdma_rxe' "${env_log}" 2>/dev/null
}

{
    printf '# RDMA Env Capability Summary\n\n'
    printf '%s\n' "- Record: \`$(basename "${record_dir}")\`"
    printf '%s\n\n' "- Generated: \`$(date -Is)\`"

    printf '## Status\n\n'
    if has_cmd_present ibv_devices && has_cmd_present ibv_devinfo && has_cmd_present rdma; then
        printf '%s\n' '- PASS_RDMA_TOOLS_PRESENT: `ibv_devices`, `ibv_devinfo`, and `rdma` are available.'
    else
        printf '%s\n' '- BLOCKED_RDMA_TOOLS_MISSING: one or more rdma-core tools are missing.'
    fi

    if has_ibv_device || has_rdma_dev; then
        printf '%s\n' '- PASS_RDMA_DEVICE_PRESENT: RDMA device was reported by verbs or rdma netlink.'
    else
        printf '%s\n' '- BLOCKED_NO_RDMA_DEVICE: no RDMA device was reported by current checks.'
    fi

    if rxe_available; then
        printf '%s\n' '- PASS_SOFT_ROCE_AVAILABLE: `rdma_rxe` module metadata is available.'
    else
        printf '%s\n' '- BLOCKED_SOFT_ROCE_UNAVAILABLE: `rdma_rxe` module metadata was not found.'
    fi

    printf '\n## Evidence Files\n\n'
    for file in ENV_CHECK.log RDMA_CAPABILITY.log SOFT_ROCE_BOUNDARY.log; do
        if [[ -f "${record_dir}/${file}" ]]; then
            printf '%s\n' "- \`${file}\`"
        else
            printf '%s\n' "- MISSING \`${file}\`"
        fi
    done

    printf '\n## Next Step\n\n'
    if has_ibv_device || has_rdma_dev; then
        printf 'Proceed to Phase 2 verbs object lifecycle on the detected RDMA device.\n'
    elif rxe_available && has_cmd_present rdma; then
        printf 'No real RDMA device was detected. Consider explicit Soft-RoCE setup with `ENABLE_RXE_SETUP=1 RXE_NETDEV=<netdev>`.\n'
    else
        printf 'Resolve missing tools or kernel support before verbs programming.\n'
    fi
} | tee "${summary}"

printf 'SUMMARY=%s\n' "${summary}"
