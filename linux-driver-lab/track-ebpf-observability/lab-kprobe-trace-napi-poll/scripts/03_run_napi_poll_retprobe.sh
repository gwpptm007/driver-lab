#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/NAPI_POLL_RETPROBE.log"
run_bpftrace "${OUT}" "${PROBE_DIR}/napi_poll_retprobe.bt" "${EBPF_DURATION}"
echo "NAPI_POLL_RETPROBE=${OUT}"
