#!/usr/bin/env bash
set -euo pipefail

# Day18 check_profile_equivalence.sh
# ----------------------------------
# 核心用途：在跑完 round2b_legacy 和 classified 后，快速回答：
# “分类表达的最终 .config / savedefconfig，是否与 legacy 结果一致？”

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
RECORDS_DIR="$SCRIPT_DIR/records"
A_PROFILE="${A_PROFILE:-round2b_legacy}"
B_PROFILE="${B_PROFILE:-classified}"
OUT_FILE="${OUT_FILE:-$RECORDS_DIR/equivalence-${A_PROFILE}-vs-${B_PROFILE}.txt}"

fail() {
    echo "[ERROR] $*" >&2
    exit 1
}

log() {
    echo "$*" | tee -a "$OUT_FILE"
}

ptr_a="$RECORDS_DIR/LAST_${A_PROFILE}.txt"
ptr_b="$RECORDS_DIR/LAST_${B_PROFILE}.txt"
[ -f "$ptr_a" ] || fail "missing pointer: $ptr_a"
[ -f "$ptr_b" ] || fail "missing pointer: $ptr_b"

dir_a=$(cat "$ptr_a")
dir_b=$(cat "$ptr_b")
[ -d "$dir_a" ] || fail "record dir not found: $dir_a"
[ -d "$dir_b" ] || fail "record dir not found: $dir_b"

cfg_a="$dir_a/build_evidence/kernel.config"
cfg_b="$dir_b/build_evidence/kernel.config"
def_a="$dir_a/build_evidence/kernel.savedefconfig"
def_b="$dir_b/build_evidence/kernel.savedefconfig"

: > "$OUT_FILE"
log "A profile : $A_PROFILE"
log "B profile : $B_PROFILE"
log "A record  : $dir_a"
log "B record  : $dir_b"
log ""

if diff -u "$cfg_a" "$cfg_b" > "$RECORDS_DIR/${A_PROFILE}-vs-${B_PROFILE}.kernel.config.diff"; then
    log "kernel.config : identical"
else
    log "kernel.config : different"
    log "diff saved to : $RECORDS_DIR/${A_PROFILE}-vs-${B_PROFILE}.kernel.config.diff"
fi

if [ -f "$def_a" ] && [ -f "$def_b" ]; then
    if diff -u "$def_a" "$def_b" > "$RECORDS_DIR/${A_PROFILE}-vs-${B_PROFILE}.savedefconfig.diff"; then
        log "savedefconfig : identical"
    else
        log "savedefconfig : different"
        log "diff saved to : $RECORDS_DIR/${A_PROFILE}-vs-${B_PROFILE}.savedefconfig.diff"
    fi
else
    log "savedefconfig : skipped (missing one side)"
fi

log ""
log "sha256 summary"
log "kernel.config  A: $(sha256sum "$cfg_a" | awk '{print $1}')"
log "kernel.config  B: $(sha256sum "$cfg_b" | awk '{print $1}')"
[ -f "$def_a" ] && log "savedefconfig A: $(sha256sum "$def_a" | awk '{print $1}')"
[ -f "$def_b" ] && log "savedefconfig B: $(sha256sum "$def_b" | awk '{print $1}')"

log ""
log "done -> $OUT_FILE"
