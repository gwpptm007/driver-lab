#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
PROFILE=${TARGET_PROFILE:-host}
RESOLVED_ENV="$ROOT_DIR/output/resolved_${PROFILE}.env"
[[ -f "$RESOLVED_ENV" ]] || TARGET_PROFILE="$PROFILE" "$ROOT_DIR/scripts/resolve_platform_env.sh" >/dev/null
# shellcheck source=/dev/null
source "$RESOLVED_ENV"

STAGE04_DIR=${STAGE04_DIR:-$(cd "$ROOT_DIR/../stage04_ring_dma" && pwd)}
LOG_FILE="$ROOT_DIR/output/build_stage04_${PROFILE}.log"
DRIVER_DIR="$STAGE04_DIR/driver"

if [[ ! -d "$DRIVER_DIR" ]]; then
    echo "[stage06] missing stage04 driver dir: $DRIVER_DIR" | tee "$LOG_FILE"
    exit 2
fi

if [[ -z "${KDIR:-}" || ! -d "${KDIR:-/nonexistent}" ]]; then
    {
        echo "[stage06] target profile: $PROFILE"
        echo "[stage06] missing or invalid KDIR: ${KDIR:-}"
        echo "[stage06] cannot build stage04 module for this target yet."
    } | tee "$LOG_FILE"
    exit 2
fi

{
    echo "[stage06] target profile: $PROFILE"
    echo "[stage06] STAGE04_DIR=$STAGE04_DIR"
    echo "[stage06] KDIR=$KDIR"
    echo "[stage06] CROSS_COMPILE=${CROSS_COMPILE:-}"
    echo "[stage06] running make build-module inside stage04..."
} | tee "$LOG_FILE"

make -C "$STAGE04_DIR" \
    ARCH="${TARGET_ARCH:-}" \
    KDIR="$KDIR" \
    CROSS_COMPILE="${CROSS_COMPILE:-}" \
    build-module | tee -a "$LOG_FILE"

echo "[stage06] build log -> $LOG_FILE"
