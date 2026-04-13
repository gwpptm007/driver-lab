#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
RESOLVED_ENV="$ROOT_DIR/output/resolved_qemu-arm64.env"
[[ -f "$RESOLVED_ENV" ]] || TARGET_PROFILE=qemu-arm64 "$ROOT_DIR/scripts/resolve_platform_env.sh" >/dev/null
# shellcheck source=/dev/null
source "$RESOLVED_ENV"

OUT_FILE="$ROOT_DIR/output/arm64_qemu_dryrun.sh"

cat > "$OUT_FILE" <<EOF
#!/usr/bin/env bash
set -euo pipefail

QEMU_BIN=${QEMU_BIN@Q}
KERNEL_IMAGE=${KERNEL_IMAGE@Q}
ROOTFS_IMAGE=${ROOTFS_IMAGE@Q}

if [[ -z "\$QEMU_BIN" ]]; then
    echo "missing QEMU_BIN"
    exit 2
fi
if [[ -z "\$KERNEL_IMAGE" ]]; then
    echo "missing KERNEL_IMAGE"
    exit 2
fi
if [[ -z "\$ROOTFS_IMAGE" ]]; then
    echo "missing ROOTFS_IMAGE"
    exit 2
fi

exec "\$QEMU_BIN" \
    -machine ${QEMU_MACHINE:-virt} \
    -cpu ${QEMU_CPU:-cortex-a57} \
    -m ${QEMU_MEMORY:-512}M \
    -nographic \
    -kernel "\$KERNEL_IMAGE" \
    -initrd "\$ROOTFS_IMAGE" \
    -append "console=ttyAMA0 rdinit=/init"
EOF
chmod +x "$OUT_FILE"
echo "[stage06] arm64 dry-run script -> $OUT_FILE"
