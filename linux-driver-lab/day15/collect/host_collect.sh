#!/usr/bin/env bash
set -euo pipefail

# Day15 host_collect.sh
# ---------------------
# 这个脚本运行在宿主机上，用来完成 Day15 baseline 的“自动采样”。
#
# 与上一版不同，这一版明确按 day15 自包含目录来工作：
# - 默认读取 day15/rootfs.img
# - 默认读取 day15/virt-day15.dtb
# - 默认统计 day15/ 下构建出的 demo_regmap.ko
# - 不再把 day13 作为默认前置条件
#
# 也就是说，推荐链路变成：
#   cd day15
#   ./build.sh          # 先构建 Day15 自己的 rootfs / dtb，并手工验证一次
#   # 退出 QEMU 回到宿主机
#   cd collect
#   ./host_collect.sh   # 再自动采集 baseline
#
# 之所以保留“先 build.sh 手工验证，再 host_collect.sh 自动采集”的两段式，
# 是因为第一次做基线实验时，这样最容易看清问题到底出在：
# - 构建链路
# - guest 脚本
# - 还是串口自动化

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
DAY15_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
REPO_ROOT=$(cd "$DAY15_DIR/.." && pwd)

SCENARIO_ID="${SCENARIO_ID:-day15-baseline-arm64-virt}"
ROOTFS_TYPE="${ROOTFS_TYPE:-busybox-initramfs}"
ARCH_NAME="${ARCH_NAME:-arm64}"
MACHINE="${MACHINE:-virt}"
CPU_MODEL="${CPU_MODEL:-cortex-a57}"
MEMORY_MB="${MEMORY_MB:-1024}"
PROMPT="${PROMPT:-/ # }"
BOOT_TIMEOUT_SEC="${BOOT_TIMEOUT_SEC:-120}"
GUEST_TIMEOUT_SEC="${GUEST_TIMEOUT_SEC:-60}"
COLLECTOR_VER="${COLLECTOR_VER:-v2}"
QEMU_BIN="${QEMU_BIN:-qemu-system-aarch64}"
MODULE_SEARCH_DIR="${MODULE_SEARCH_DIR:-$DAY15_DIR}"
KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../kernel-src/linux-5.15.10}"
KDIR="${KDIR:-$KERNEL_DIR/build/arm64}"
KERNEL_IMG="${KERNEL_IMG:-$KERNEL_DIR/output/arm64/Image}"
ROOTFS_IMG="${ROOTFS_IMG:-$DAY15_DIR/rootfs.img}"
DTB_IMG="${DTB_IMG:-$DAY15_DIR/virt-day15.dtb}"
RECORDS_ROOT="${RECORDS_ROOT:-$DAY15_DIR/records}"
CSV_TEMPLATE="${CSV_TEMPLATE:-$DAY15_DIR/baseline_template.csv}"
GUEST_SCRIPT_PATH="${GUEST_SCRIPT_PATH:-/bin/day15_guest_collect.sh}"
GUEST_OUTDIR="${GUEST_OUTDIR:-/tmp/day15-baseline}"
DEMO_MODULE="${DEMO_MODULE:-demo_regmap.ko}"
KERNEL_CMDLINE="${KERNEL_CMDLINE:-console=ttyAMA0 root=/dev/ram0 rw rdinit=/init}"

now_ms() {
    date +%s%3N
}

fail() {
    echo "[ERROR] $*" >&2
    exit 1
}

info() {
    echo "[INFO] $*"
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || fail "command not found: $1"
}

wait_for_pattern() {
    local file="$1"
    local pattern="$2"
    local timeout_sec="$3"
    local waited=0

    while (( waited < timeout_sec )); do
        if [ -f "$file" ] && grep -Fq "$pattern" "$file"; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done

    return 1
}

send_cmd() {
    local cmd="$1"
    printf '%s\n' "$cmd" >&3
}

extract_env_block() {
    local log_file="$1"
    local out_file="$2"

    awk '
        $0 == "__DAY15_ENV_BEGIN__" { in_block=1; next }
        $0 == "__DAY15_ENV_END__"   { in_block=0; exit }
        in_block { print }
    ' "$log_file" > "$out_file"
}

extract_named_block() {
    local log_file="$1"
    local block_name="$2"
    local out_file="$3"

    awk -v name="$block_name" '
        $0 == "__DAY15_FILE_BEGIN__ " name { in_block=1; next }
        $0 == "__DAY15_FILE_END__ " name   { in_block=0; exit }
        in_block { print }
    ' "$log_file" > "$out_file"
}

shell_escape_csv() {
    local s="$1"
    s=${s//\"/\"\"}
    printf '"%s"' "$s"
}

require_cmd "$QEMU_BIN"
require_cmd stat
require_cmd grep
require_cmd awk
require_cmd tee
require_cmd mkfifo
require_cmd date

[ -f "$KERNEL_IMG" ] || fail "kernel image not found: $KERNEL_IMG"
[ -f "$ROOTFS_IMG" ] || fail "rootfs image not found: $ROOTFS_IMG (请先在 day15/ 执行 ./build.sh)"
[ -f "$DTB_IMG" ] || fail "dtb not found: $DTB_IMG (请先在 day15/ 执行 ./build.sh)"
[ -f "$CSV_TEMPLATE" ] || fail "baseline template not found: $CSV_TEMPLATE"

STAMP=$(date +%Y%m%d-%H%M%S)
RECORD_DIR="$RECORDS_ROOT/$STAMP-$SCENARIO_ID"
mkdir -p "$RECORD_DIR"

SERIAL_LOG="$RECORD_DIR/serial.log"
HOST_ENV="$RECORD_DIR/host_metrics.env"
GUEST_ENV="$RECORD_DIR/guest_metrics.env"
MERGED_ENV="$RECORD_DIR/metrics.env"
BASELINE_CSV="$RECORD_DIR/baseline.csv"
FIFO_IN="$RECORD_DIR/qemu.in"
MEMINFO_TXT="$RECORD_DIR/meminfo.txt"
MODULES_TXT="$RECORD_DIR/modules.txt"
TRACERS_TXT="$RECORD_DIR/available_tracers.txt"
DMESG_TAIL_TXT="$RECORD_DIR/dmesg_tail.txt"
SNAPSHOT_TXT="$RECORD_DIR/snapshot.txt"
MOUNT_TXT="$RECORD_DIR/mount.txt"
FILESYSTEMS_TXT="$RECORD_DIR/filesystems.txt"

info "record dir          : $RECORD_DIR"
info "kernel image        : $KERNEL_IMG"
info "rootfs image        : $ROOTFS_IMG"
info "dtb image           : $DTB_IMG"
info "qemu                : $QEMU_BIN"

image_bytes=$(stat -c '%s' "$KERNEL_IMG")
rootfs_bytes=$(stat -c '%s' "$ROOTFS_IMG")
image_kib=$(( (image_bytes + 1023) / 1024 ))
rootfs_kib=$(( (rootfs_bytes + 1023) / 1024 ))
modules_built_count=$(find "$MODULE_SEARCH_DIR" -type f -name '*.ko' 2>/dev/null | wc -l | tr -d ' ')
kernel_ver="unknown"

if [ -d "$KERNEL_DIR" ] && [ -d "$KDIR" ] && [ -f "$KERNEL_DIR/Makefile" ]; then
    kernel_ver=$(make -s -C "$KERNEL_DIR" O="$KDIR" kernelrelease 2>/dev/null || echo unknown)
fi

cat > "$HOST_ENV" <<EOF2
scenario_id=$SCENARIO_ID
kernel_ver=$kernel_ver
arch=$ARCH_NAME
machine=$MACHINE
rootfs_type=$ROOTFS_TYPE
demo_module=$DEMO_MODULE
collector_ver=$COLLECTOR_VER
image_bytes=$image_bytes
image_kib=$image_kib
rootfs_bytes=$rootfs_bytes
rootfs_kib=$rootfs_kib
modules_built_count=$modules_built_count
boot_start_event=qemu_process_start
boot_end_event=first_shell_prompt
boot_note='QEMU start to first serial prompt $PROMPT'
EOF2

mkfifo "$FIFO_IN"
boot_start_ms=$(now_ms)

"$QEMU_BIN" \
    -machine "$MACHINE" \
    -cpu "$CPU_MODEL" \
    -m "$MEMORY_MB" \
    -nographic \
    -kernel "$KERNEL_IMG" \
    -dtb "$DTB_IMG" \
    -initrd "$ROOTFS_IMG" \
    -append "$KERNEL_CMDLINE" \
    < "$FIFO_IN" \
    2>&1 | tee "$SERIAL_LOG" &
QEMU_PIPE_PID=$!

exec 3> "$FIFO_IN"

cleanup() {
    set +e
    if [ -n "${QEMU_PIPE_PID:-}" ] && kill -0 "$QEMU_PIPE_PID" 2>/dev/null; then
        kill "$QEMU_PIPE_PID" 2>/dev/null || true
        wait "$QEMU_PIPE_PID" 2>/dev/null || true
    fi
    exec 3>&- || true
    rm -f "$FIFO_IN"
}
trap cleanup EXIT

info "waiting for first shell prompt ..."
if ! wait_for_pattern "$SERIAL_LOG" "$PROMPT" "$BOOT_TIMEOUT_SEC"; then
    fail "guest prompt not found within ${BOOT_TIMEOUT_SEC}s; check $SERIAL_LOG"
fi
boot_end_ms=$(now_ms)
boot_ms=$((boot_end_ms - boot_start_ms))
info "boot_ms             : $boot_ms"

send_cmd "$GUEST_SCRIPT_PATH '$GUEST_OUTDIR'"

info "waiting for guest metrics marker ..."
if ! wait_for_pattern "$SERIAL_LOG" "__DAY15_ENV_END__" "$GUEST_TIMEOUT_SEC"; then
    fail "guest metrics marker not found; maybe $GUEST_SCRIPT_PATH is missing in rootfs, check $SERIAL_LOG"
fi

extract_env_block "$SERIAL_LOG" "$GUEST_ENV"
extract_named_block "$SERIAL_LOG" "mount.txt" "$MOUNT_TXT"
extract_named_block "$SERIAL_LOG" "filesystems.txt" "$FILESYSTEMS_TXT"
extract_named_block "$SERIAL_LOG" "meminfo.txt" "$MEMINFO_TXT"
extract_named_block "$SERIAL_LOG" "modules.txt" "$MODULES_TXT"
extract_named_block "$SERIAL_LOG" "available_tracers.txt" "$TRACERS_TXT"
extract_named_block "$SERIAL_LOG" "dmesg_tail.txt" "$DMESG_TAIL_TXT"
extract_named_block "$SERIAL_LOG" "snapshot.txt" "$SNAPSHOT_TXT"

if [ -s "$MEMINFO_TXT" ]; then
    awk -f "$SCRIPT_DIR/parse_meminfo.awk" "$MEMINFO_TXT" > "$RECORD_DIR/meminfo_from_host.env"
fi

# shellcheck disable=SC1090
source "$HOST_ENV"
# shellcheck disable=SC1090
source "$GUEST_ENV"
if [ -f "$RECORD_DIR/meminfo_from_host.env" ]; then
    # shellcheck disable=SC1090
    source "$RECORD_DIR/meminfo_from_host.env"
fi

cat > "$MERGED_ENV" <<EOF2
scenario_id=$scenario_id
kernel_ver=$kernel_ver
arch=$arch
machine=$machine
rootfs_type=$rootfs_type
demo_module=$demo_module
collector_ver=$collector_ver
image_bytes=$image_bytes
image_kib=$image_kib
rootfs_bytes=$rootfs_bytes
rootfs_kib=$rootfs_kib
modules_built_count=$modules_built_count
boot_start_event=$boot_start_event
boot_end_event=$boot_end_event
boot_ms=$boot_ms
boot_note='$boot_note'
memtotal_kib=$memtotal_kib
memfree_kib=$memfree_kib
memavailable_kib=$memavailable_kib
slab_kib=$slab_kib
sreclaimable_kib=$sreclaimable_kib
sunreclaim_kib=$sunreclaim_kib
kernelstack_kib=$kernelstack_kib
pagetables_kib=$pagetables_kib
modules_loaded_count=$modules_loaded_count
debugfs_ok=$debugfs_ok
tracing_ok=$tracing_ok
function_graph_ok=$function_graph_ok
trace_smoke_ok=$trace_smoke_ok
perf_bin_ok=$perf_bin_ok
perf_smoke_ok=$perf_smoke_ok
boot_ok=$boot_ok
insmod_ok=$insmod_ok
snapshot_ok=$snapshot_ok
trigger_ok=$trigger_ok
dmesg_warn=$dmesg_warn
remarks='$remarks'
EOF2

header=$(cat "$CSV_TEMPLATE")
printf '%s\n' "$header" > "$BASELINE_CSV"

row=(
    "$scenario_id"
    "$kernel_ver"
    "$arch"
    "$machine"
    "$rootfs_type"
    "$demo_module"
    "$collector_ver"
    "$image_bytes"
    "$image_kib"
    "$rootfs_bytes"
    "$rootfs_kib"
    "$modules_built_count"
    "$boot_start_event"
    "$boot_end_event"
    "$boot_ms"
    "$boot_note"
    "$memtotal_kib"
    "$memfree_kib"
    "$memavailable_kib"
    "$slab_kib"
    "$sreclaimable_kib"
    "$sunreclaim_kib"
    "$kernelstack_kib"
    "$pagetables_kib"
    "$modules_loaded_count"
    "$debugfs_ok"
    "$tracing_ok"
    "$function_graph_ok"
    "$trace_smoke_ok"
    "$perf_bin_ok"
    "$perf_smoke_ok"
    "$boot_ok"
    "$insmod_ok"
    "$snapshot_ok"
    "$trigger_ok"
    "$dmesg_warn"
    "$remarks"
)

for idx in "${!row[@]}"; do
    if (( idx > 0 )); then
        printf ',' >> "$BASELINE_CSV"
    fi
    shell_escape_csv "${row[$idx]}" >> "$BASELINE_CSV"
done
printf '\n' >> "$BASELINE_CSV"

send_cmd "poweroff -f || reboot -f"
sleep 2

info "host env            : $HOST_ENV"
info "guest env           : $GUEST_ENV"
info "merged env          : $MERGED_ENV"
info "baseline csv        : $BASELINE_CSV"
info "serial log          : $SERIAL_LOG"
info "done"
