#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/SOFTIRQ_NAPI_CORRELATION.log"
run_bpftrace "${OUT}" "${PROBE_DIR}/softirq_napi_correlation.bt" "${EBPF_DURATION}"
echo "SOFTIRQ_NAPI_CORRELATION=${OUT}"
