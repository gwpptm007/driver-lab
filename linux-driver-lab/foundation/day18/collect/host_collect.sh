#!/usr/bin/env bash
set -euo pipefail

# Day18 host_collect.sh
# ---------------------
# 这个脚本运行在宿主机上，用来完成 Day18 的自动化采样与归档。
#
# 它做的事情可以概括为四段：
#
# 1. 启动 QEMU，并等待第一个 shell prompt
# 2. 通过串口自动执行 guest_collect.sh
# 3. 从 serial.log 里提取 metrics.env 和关键文本块
# 4. 合并 host + guest 指标，生成 records/<timestamp>-<scenario>/baseline.csv
#
# Day18 版本最关键的变化是：
# - 默认读取 day18/rootfs.img
# - 默认读取 day18/virt-day18.dtb
# - 默认使用 /bin/day18_guest_collect.sh
# - marker 全部改为 __DAY18_*，不会再和 day15/day16 混淆
# - 继续兼容 baseline / round2b_legacy / classified 三种 scenario，只要你修改 SCENARIO_ID 即可
#
# 推荐用法：
#   cd linux-driver-lab/day18/collect
#   SCENARIO_ID=day18-baseline-arm64-virt ./host_collect.sh
#   SCENARIO_ID=day18-round2b_legacy-arm64-virt   ./host_collect.sh
#   SCENARIO_ID=day18-classified-arm64-virt  ./host_collect.sh

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
DAY18_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
REPO_ROOT=$(cd "$DAY18_DIR/.." && pwd)

SCENARIO_ID="${SCENARIO_ID:-day18-baseline-arm64-virt}"
ROOTFS_TYPE="${ROOTFS_TYPE:-busybox-initramfs}"
ARCH_NAME="${ARCH_NAME:-arm64}"
MACHINE="${MACHINE:-virt}"
CPU_MODEL="${CPU_MODEL:-cortex-a57}"
MEMORY_MB="${MEMORY_MB:-1024}"
# 默认 prompt 改成 ~ # ，因为 BusyBox/ash 在本实验 rootfs 中更常见是这个样式。
#
# 但仅仅改默认值还不够稳：不同 init 脚本、不同 shell、不同 PS1 配置下，
# prompt 可能是：
#   ~ #
#   / #
#   #
# 因此 Day18 这一版同时支持一个“候选 prompt 列表”，只要 serial.log
# 中出现任意一个，就认为 guest 已经进入可交互 shell。
PROMPT="${PROMPT:-~ # }"
PROMPT_CANDIDATES="${PROMPT_CANDIDATES:-$PROMPT|/ # }"
BOOT_TIMEOUT_SEC="${BOOT_TIMEOUT_SEC:-120}"
GUEST_TIMEOUT_SEC="${GUEST_TIMEOUT_SEC:-60}"
HANDSHAKE_ATTEMPTS="${HANDSHAKE_ATTEMPTS:-5}"
HANDSHAKE_WAIT_SEC="${HANDSHAKE_WAIT_SEC:-6}"
COLLECTOR_VER="${COLLECTOR_VER:-v4}"
QEMU_BIN="${QEMU_BIN:-qemu-system-aarch64}"
MODULE_SEARCH_DIR="${MODULE_SEARCH_DIR:-$DAY18_DIR}"
KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../kernel-src/linux-5.15.10}"
KDIR="${KDIR:-$KERNEL_DIR/build/arm64}"
KERNEL_IMG="${KERNEL_IMG:-$KERNEL_DIR/output/arm64/Image}"
ROOTFS_IMG="${ROOTFS_IMG:-$DAY18_DIR/rootfs.img}"
DTB_IMG="${DTB_IMG:-$DAY18_DIR/virt-day18.dtb}"
RECORDS_ROOT="${RECORDS_ROOT:-$DAY18_DIR/records}"
CSV_TEMPLATE="${CSV_TEMPLATE:-$DAY18_DIR/baseline_template.csv}"
GUEST_SCRIPT_PATH="${GUEST_SCRIPT_PATH:-/bin/day18_guest_collect.sh}"
GUEST_OUTDIR="${GUEST_OUTDIR:-/tmp/day18-baseline}"
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

# 等待“任意一个” prompt 出现。
#
# 参数 2 采用竖线分隔，例如：
#   "~ # |/ # |# "
#
# 这样做的目的，是避免把 host_collect.sh 绑死在某一个固定 PS1 上。
# 只要 guest 已经进入可交互 shell，后续通过串口下发命令就可以继续。
wait_for_any_pattern() {
    local file="$1"
    local patterns="$2"
    local timeout_sec="$3"
    local waited=0
    local pat
    local old_ifs="$IFS"

    while (( waited < timeout_sec )); do
        if [ -f "$file" ]; then
            IFS='|'
            for pat in $patterns; do
                # 去掉两端多余空白，避免用户把 PROMPT_CANDIDATES 写成带空格分隔的形式时匹配失败。
                pat=$(printf '%s' "$pat" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')
                [ -n "$pat" ] || continue
                if grep -Fq "$pat" "$file"; then
                    IFS="$old_ifs"
                    return 0
                fi
            done
            IFS="$old_ifs"
        fi
        sleep 1
        waited=$((waited + 1))
    done

    IFS="$old_ifs"
    return 1
}

send_cmd() {
    local cmd="$1"
    # 串口/控制台自动化里，很多 shell 对回车比单纯的换行更稳。
    # 这里统一发送 CRLF，兼容 ash/sh 在串口上的常见交互行为。
    printf '%s\r\n' "$cmd" >&3
}

send_empty_enter() {
    # 发送一个“空回车”，用于唤醒 shell、把当前 prompt 刷出来。
    printf '\r\n' >&3
}

wait_for_handshake() {
    local log_file="$1"
    local token="$2"
    local timeout_sec="$3"
    wait_for_pattern "$log_file" "$token" "$timeout_sec"
}

retry_serial_handshake() {
    local log_file="$1"
    local attempts="$2"
    local wait_sec="$3"
    local token_base="__DAY18_HOST_HANDSHAKE__"
    local token=""
    local i=1

    while (( i <= attempts )); do
        token="${token_base}${i}"
        info "serial handshake attempt : $i/$attempts token=$token"

        # 先发空回车，把 prompt 再刷一次；有些场景第一次 prompt 已经打印出来，
        # 但 shell 输入通路还没完全稳定，紧跟着发命令容易丢。
        send_empty_enter
        sleep 1

        # 再等一次 prompt，确认当前确实又回到了可交互状态。
        wait_for_any_pattern "$log_file" "$PROMPT_CANDIDATES" 3 || true

        # 连发两次轻量 token，降低单次串口注入丢失的概率。
        send_cmd "echo $token"
        sleep 1
        send_cmd "echo $token"

        if wait_for_handshake "$log_file" "$token" "$wait_sec"; then
            printf '%s
' "$token"
            return 0
        fi

        i=$((i + 1))
    done

    return 1
}

extract_env_block() {
    local log_file="$1"
    local out_file="$2"

    # 串口日志经常带 CRLF、回显残留，甚至在某些时刻 marker 前面还会混入 prompt。
    # 因此这里不要再用“整行必须完全等于 marker”的严格写法，
    # 而是在逐行去掉 \r 之后，只要该行包含 marker 就切换提取状态。
    awk '
        {
            line=$0
            gsub(/\r/, "", line)
        }
        index(line, "__DAY18_ENV_BEGIN__") { in_block=1; next }
        index(line, "__DAY18_ENV_END__")   { in_block=0; exit }
        in_block { print line }
    ' "$log_file" > "$out_file"
}

extract_named_block() {
    local log_file="$1"
    local block_name="$2"
    local out_file="$3"

    # 同样先去掉串口日志里的 \r，再按 begin/end marker 抽取命名文件块。
    awk -v name="$block_name" '
        {
            line=$0
            gsub(/\r/, "", line)
        }
        index(line, "__DAY18_FILE_BEGIN__ " name) { in_block=1; next }
        index(line, "__DAY18_FILE_END__ " name)   { in_block=0; exit }
        in_block { print line }
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
require_cmd sed
require_cmd tee
require_cmd mkfifo
require_cmd date

[ -f "$KERNEL_IMG" ] || fail "kernel image not found: $KERNEL_IMG"
[ -f "$ROOTFS_IMG" ] || fail "rootfs image not found: $ROOTFS_IMG (请先在 day18/ 执行 ./build.sh)"
[ -f "$DTB_IMG" ] || fail "dtb not found: $DTB_IMG (请先在 day18/ 执行 ./build.sh)"
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
PERF_VERSION_TXT="$RECORD_DIR/perf_version.txt"
PERF_LIST_TXT="$RECORD_DIR/perf_list.txt"
PERF_STAT_TXT="$RECORD_DIR/perf_stat.txt"
PERF_MANIFEST_TXT="$RECORD_DIR/perf_manifest.txt"
TRACING_DIR_TXT="$RECORD_DIR/tracing_dir.txt"
GUEST_CMD_RC_TXT="$RECORD_DIR/guest_cmd_rc.txt"

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

"$QEMU_BIN"     -machine "$MACHINE"     -cpu "$CPU_MODEL"     -m "$MEMORY_MB"     -nographic     -kernel "$KERNEL_IMG"     -dtb "$DTB_IMG"     -initrd "$ROOTFS_IMG"     -append "$KERNEL_CMDLINE"     < "$FIFO_IN"     2>&1 | tee "$SERIAL_LOG" &
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
info "prompt candidates   : $PROMPT_CANDIDATES"
if ! wait_for_any_pattern "$SERIAL_LOG" "$PROMPT_CANDIDATES" "$BOOT_TIMEOUT_SEC"; then
    fail "guest prompt not found within ${BOOT_TIMEOUT_SEC}s; check $SERIAL_LOG and verify PROMPT/PROMPT_CANDIDATES"
fi
boot_end_ms=$(now_ms)
boot_ms=$((boot_end_ms - boot_start_ms))
info "boot_ms             : $boot_ms"

# 不直接在“第一次看到 prompt”后立刻发送采样命令。
# 现场现象表明：串口有时虽然已经打印出 prompt，但 shell 的输入通路尚未稳定，
# 第一条命令可能丢失，随后 host_collect.sh 一直等不到 __DAY18_ENV_END__。
#
# 这里改成“可重试握手”：
# 1. 先发空回车，把 prompt 再刷出来；
# 2. 再次确认 prompt 仍然存在；
# 3. 连续发送轻量 echo token；
# 4. 只要 serial.log 里出现任一握手 token，就认为输入链稳定。
HANDSHAKE_TOKEN=""
info "waiting for host/guest serial handshake ..."
if ! HANDSHAKE_TOKEN=$(retry_serial_handshake "$SERIAL_LOG" "$HANDSHAKE_ATTEMPTS" "$HANDSHAKE_WAIT_SEC"); then
    fail "serial handshake token not observed after ${HANDSHAKE_ATTEMPTS} attempts; guest prompt may have matched too early or shell input is not stable yet, check $SERIAL_LOG"
fi

# 通过 /bin/sh -c 包一层，兼容脚本路径、参数和重定向等 shell 解析行为。
# 再追加一个退出码 token，便于后续从 serial.log 直接判断 guest_collect 是否被成功执行。
GUEST_CMD_RC_TOKEN="__DAY18_GUEST_CMD_RC__"
send_cmd "/bin/sh -c '$GUEST_SCRIPT_PATH '\''$GUEST_OUTDIR'\''; rc=\$?; echo $GUEST_CMD_RC_TOKEN\$rc'"

info "waiting for guest metrics marker ..."
if ! wait_for_pattern "$SERIAL_LOG" "__DAY18_ENV_END__" "$GUEST_TIMEOUT_SEC"; then
    fail "guest metrics marker not found; maybe command injection failed or $GUEST_SCRIPT_PATH did not run, check $SERIAL_LOG for $HANDSHAKE_TOKEN / $GUEST_CMD_RC_TOKEN"
fi

extract_env_block "$SERIAL_LOG" "$GUEST_ENV"
extract_named_block "$SERIAL_LOG" "mount.txt" "$MOUNT_TXT"
extract_named_block "$SERIAL_LOG" "filesystems.txt" "$FILESYSTEMS_TXT"
extract_named_block "$SERIAL_LOG" "meminfo.txt" "$MEMINFO_TXT"
extract_named_block "$SERIAL_LOG" "modules.txt" "$MODULES_TXT"
extract_named_block "$SERIAL_LOG" "available_tracers.txt" "$TRACERS_TXT"
extract_named_block "$SERIAL_LOG" "dmesg_tail.txt" "$DMESG_TAIL_TXT"
extract_named_block "$SERIAL_LOG" "snapshot.txt" "$SNAPSHOT_TXT"
extract_named_block "$SERIAL_LOG" "perf_version.txt" "$PERF_VERSION_TXT"
extract_named_block "$SERIAL_LOG" "perf_list.txt" "$PERF_LIST_TXT"
extract_named_block "$SERIAL_LOG" "perf_stat.txt" "$PERF_STAT_TXT"
extract_named_block "$SERIAL_LOG" "perf_manifest.txt" "$PERF_MANIFEST_TXT"
extract_named_block "$SERIAL_LOG" "tracing_dir.txt" "$TRACING_DIR_TXT"
grep -o "__DAY18_GUEST_CMD_RC__[0-9][0-9]*" "$SERIAL_LOG" | tail -n 1 > "$GUEST_CMD_RC_TXT" 2>/dev/null || true

# 这一步很关键：如果 marker 明明已经在 serial.log 里，但抽出来的 guest env 仍然是空，
# 说明不是 guest_collect 没执行，而是 host 侧日志解析失败。
# 这里直接给出明确错误，避免后面 source 空文件后再以“unbound variable”这种间接形式报错。
if [ ! -s "$GUEST_ENV" ]; then
    fail "guest env block extracted as empty; serial markers were seen but parsing failed, check $SERIAL_LOG around __DAY18_ENV_BEGIN__/__DAY18_ENV_END__"
fi

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
printf '\\n' >> "$BASELINE_CSV"

send_cmd "poweroff -f || reboot -f"
sleep 2

info "host env            : $HOST_ENV"
info "guest env           : $GUEST_ENV"
info "merged env          : $MERGED_ENV"
info "baseline csv        : $BASELINE_CSV"
info "guest cmd rc token  : $GUEST_CMD_RC_TXT"
info "serial log          : $SERIAL_LOG"
info "done"
