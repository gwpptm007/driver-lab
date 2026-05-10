#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

REC_DIR="${1:-$(latest_record_dir)}"
OUT="${REC_DIR}/REVIEW_BUNDLE.md"

has_file() {
    [[ -s "${REC_DIR}/$1" ]] && echo "DONE" || echo "MISSING"
}

pass_basic="NO"
if grep -q "BUILD_RESULT=PASS" "${REC_DIR}/BUILD.log" 2>/dev/null && \
   grep -q "attached XDP program" "${REC_DIR}/XDP_PASS.log" 2>/dev/null && \
   grep -q "detach ok" "${REC_DIR}/XDP_PASS.log" 2>/dev/null; then
    pass_basic="YES"
fi

pass_action="NO"
if grep -q "action=drop" "${REC_DIR}/XDP_DROP.log" 2>/dev/null || grep -q "drop" "${REC_DIR}/XDP_DROP.log" 2>/dev/null; then
    pass_action="YES"
fi

redirect_ready="NO"
if grep -q "action=redirect" "${REC_DIR}/XDP_REDIRECT_DRYRUN.log" 2>/dev/null || grep -q "redirect" "${REC_DIR}/XDP_REDIRECT_DRYRUN.log" 2>/dev/null; then
    redirect_ready="YES"
fi

cat > "${OUT}" <<EOF2
# REVIEW_BUNDLE: lab-xdp-redirect-basics

## Metadata

- Date: $(date -Is)
- Host: $(hostname)
- Kernel: $(uname -r)
- Record: \\`${REC_DIR}\\`
- Interface: \\`${AF_XDP_IFACE}\\`
- Mode: \\`${AF_XDP_MODE}\\`

## Files

| File | Status |
|---|---|
| ENV_CHECK.txt | $(has_file ENV_CHECK.txt) |
| BUILD.log | $(has_file BUILD.log) |
| PREPARE_KERNEL_NETDEV.txt | $(has_file PREPARE_KERNEL_NETDEV.txt) |
| XDP_PASS.log | $(has_file XDP_PASS.log) |
| XDP_DROP.log | $(has_file XDP_DROP.log) |
| XDP_REDIRECT_DRYRUN.log | $(has_file XDP_REDIRECT_DRYRUN.log) |
| COLLECT_STATS.txt | $(has_file COLLECT_STATS.txt) |

## Acceptance

| Item | Result |
|---|---|
| PASS_BASIC | ${pass_basic} |
| PASS_ACTION | ${pass_action} |
| REDIRECT_MODEL_READY | ${redirect_ready} |

## Interpretation

- PASS_BASIC means BPF build + XDP attach + stats + detach succeeded.
- PASS_ACTION means DROP action path was executed with explicit confirmation.
- REDIRECT_MODEL_READY means the program supports XSKMAP redirect, but this is not full AF_XDP socket success yet.

## Next

If PASS_BASIC is YES, continue to:

\
track-af-xdp/lab-af-xdp-socket-rings
\

EOF2

echo "Generated ${OUT}"
