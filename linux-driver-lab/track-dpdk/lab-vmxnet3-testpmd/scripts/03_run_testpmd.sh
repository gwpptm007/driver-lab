#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root_for_write
guard_not_mgmt_pci

: "${TESTPMD_SECONDS:=20}"
: "${TESTPMD_CORES:=0-1}"
: "${TESTPMD_MEM_CHANNELS:=4}"
: "${TESTPMD_FORWARD_MODE:=io}"
: "${TESTPMD_STATS_PERIOD:=5}"
: "${TESTPMD_EXTRA_EAL:=}"
: "${TESTPMD_EXTRA_ARGS:=}"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

testpmd="$(find_testpmd || true)"
if [[ -z "${testpmd}" ]]; then
    echo "ERROR: testpmd not found. Install dpdk-testpmd or set TESTPMD_BIN=/path/to/dpdk-testpmd" >&2
    exit 1
fi

OUT="${RECORD_DIR}/TESTPMD.log"
: > "${OUT}"

{
    echo "# TESTPMD"
    echo "date=$(date '+%F %T')"
    echo "testpmd=${testpmd}"
    echo "DPDK_PCI=${DPDK_PCI}"
    echo "TESTPMD_SECONDS=${TESTPMD_SECONDS}"
    echo "TESTPMD_CORES=${TESTPMD_CORES}"
    echo "TESTPMD_MEM_CHANNELS=${TESTPMD_MEM_CHANNELS}"
    echo "TESTPMD_FORWARD_MODE=${TESTPMD_FORWARD_MODE}"
    echo
} >> "${OUT}"

CMD=(
    "${testpmd}"
    -l "${TESTPMD_CORES}"
    -n "${TESTPMD_MEM_CHANNELS}"
    -a "${DPDK_PCI}"
)

# shellcheck disable=SC2206
EXTRA_EAL=( ${TESTPMD_EXTRA_EAL} )
if [[ "${#EXTRA_EAL[@]}" -gt 0 ]]; then
    CMD+=( "${EXTRA_EAL[@]}" )
fi

CMD+=(
    --
    --port-topology=chained
    --forward-mode="${TESTPMD_FORWARD_MODE}"
    --auto-start
    --stats-period="${TESTPMD_STATS_PERIOD}"
)

# shellcheck disable=SC2206
EXTRA_ARGS=( ${TESTPMD_EXTRA_ARGS} )
if [[ "${#EXTRA_ARGS[@]}" -gt 0 ]]; then
    CMD+=( "${EXTRA_ARGS[@]}" )
fi

{
    echo "## Command"
    printf '%q ' timeout "${TESTPMD_SECONDS}" "${CMD[@]}"
    echo
    echo
    echo "## Output"
} >> "${OUT}"

set +e
timeout "${TESTPMD_SECONDS}" "${CMD[@]}" >> "${OUT}" 2>&1
rc=$?
set -e

{
    echo
    echo "## Exit"
    echo "timeout/testpmd rc=${rc}"
    if [[ "${rc}" -eq 124 ]]; then
        echo "NOTE: rc=124 means timeout stopped testpmd after TESTPMD_SECONDS; this is acceptable for timed smoke test."
    fi
} >> "${OUT}"

if [[ "${rc}" -ne 0 && "${rc}" -ne 124 ]]; then
    echo "ERROR: testpmd failed rc=${rc}. See ${OUT}" >&2
    exit "${rc}"
fi

cat <<EOF

[OK] testpmd smoke log saved:
${OUT}

Next:
  ./scripts/04_collect_stats.sh
EOF
