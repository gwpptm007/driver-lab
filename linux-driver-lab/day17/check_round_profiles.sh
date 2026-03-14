#!/usr/bin/env bash
set -euo pipefail

DAY17_DIR="$(cd "$(dirname "$0")" && pwd)"
RECORDS_DIR="$DAY17_DIR/records"
OUT_FILE="$RECORDS_DIR/profile_check_$(date +%Y%m%d-%H%M%S).log"

log() {
    echo "$@" | tee -a "$OUT_FILE"
}

section() {
    echo | tee -a "$OUT_FILE"
    echo "============================================================" | tee -a "$OUT_FILE"
    echo "$@" | tee -a "$OUT_FILE"
    echo "============================================================" | tee -a "$OUT_FILE"
}

need_file() {
    local f="$1"
    if [[ ! -e "$f" ]]; then
        echo "[ERROR] missing: $f" | tee -a "$OUT_FILE"
        exit 1
    fi
}

BASE_PTR="$RECORDS_DIR/LAST_baseline.txt"
R1_PTR="$RECORDS_DIR/LAST_round1.txt"
R2_PTR="$RECORDS_DIR/LAST_round2b.txt"
need_file "$BASE_PTR"; need_file "$R1_PTR"; need_file "$R2_PTR"
BASE_DIR="$(cat "$BASE_PTR")"
R1_DIR="$(cat "$R1_PTR")"
R2_DIR="$(cat "$R2_PTR")"
BASE_EVID="$BASE_DIR/build_evidence"
R1_EVID="$R1_DIR/build_evidence"
R2_EVID="$R2_DIR/build_evidence"
BASE_CFG="$BASE_EVID/kernel.config"
R1_CFG="$R1_EVID/kernel.config"
R2_CFG="$R2_EVID/kernel.config"
BASE_ENV="$BASE_EVID/artifact_evidence.env"
R1_ENV="$R1_EVID/artifact_evidence.env"
R2_ENV="$R2_EVID/artifact_evidence.env"
need_file "$BASE_CFG"; need_file "$R1_CFG"; need_file "$R2_CFG"
need_file "$BASE_ENV"; need_file "$R1_ENV"; need_file "$R2_ENV"

section "1. LAST pointers"
log "baseline : $BASE_DIR"
log "round1   : $R1_DIR"
log "round2b  : $R2_DIR"

section "2. artifact_evidence.env"
for f in "$BASE_ENV" "$R1_ENV" "$R2_ENV"; do
    log "--- $f ---"
    cat "$f" | tee -a "$OUT_FILE"
done

section "3. applied_fragments.txt"
for f in "$BASE_EVID/applied_fragments.txt" "$R1_EVID/applied_fragments.txt" "$R2_EVID/applied_fragments.txt"; do
    if [[ -f "$f" ]]; then
        log "--- $f ---"
        cat "$f" | tee -a "$OUT_FILE"
    else
        log "--- $f (missing) ---"
    fi
done

section "4. kernel.config sha256"
sha256sum "$BASE_CFG" "$R1_CFG" "$R2_CFG" | tee -a "$OUT_FILE"

section "5. diff baseline vs round1"
if diff -u "$BASE_CFG" "$R1_CFG" > /tmp/day17_diff_b_r1.txt; then
    log "NO DIFF"
else
    sed -n '1,200p' /tmp/day17_diff_b_r1.txt | tee -a "$OUT_FILE"
fi

section "6. diff round1 vs round2b"
if diff -u "$R1_CFG" "$R2_CFG" > /tmp/day17_diff_r1_r2.txt; then
    log "NO DIFF"
else
    sed -n '1,200p' /tmp/day17_diff_r1_r2.txt | tee -a "$OUT_FILE"
fi

section "7. summary"
BASE_SHA="$(sha256sum "$BASE_CFG" | awk '{print $1}')"
R1_SHA="$(sha256sum "$R1_CFG" | awk '{print $1}')"
R2_SHA="$(sha256sum "$R2_CFG" | awk '{print $1}')"
log "baseline config sha : $BASE_SHA"
log "round1   config sha : $R1_SHA"
log "round2b  config sha : $R2_SHA"
if [[ "$BASE_SHA" == "$R1_SHA" && "$R1_SHA" == "$R2_SHA" ]]; then
    log "[SUMMARY] three kernel.config files are IDENTICAL"
else
    log "[SUMMARY] kernel.config files are DIFFERENT across profiles"
fi
log
log "[DONE] report saved to: $OUT_FILE"
