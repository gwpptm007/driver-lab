#!/bin/sh
set -eu

# Day20 guest_day20_common.sh
# ----------------------------
# 这份脚本不是单独运行的主入口，而是给 smoke/trace/perf 三类脚本复用。
# Day20 的思路是：
# 1. 先在 guest 里把 /proc /sys /dev /debugfs /tracefs 准备好；
# 2. 把每一类检查结果写进 /tmp/day20/pass_fail.env；
# 3. 把原始文本材料（snapshot / trace / perf / dmesg）留在 /tmp/day20；
# 4. 宿主机侧回收这些材料，再归档到 day20/records/。

DAY20_OUTDIR="${DAY20_OUTDIR:-/tmp/day20}"
DEBUGFS_ROOT="${DEBUGFS_ROOT:-/sys/kernel/debug}"
TRACEFS_ROOT="${TRACEFS_ROOT:-/sys/kernel/tracing}"
TRACING_DIR=""
PASS_ENV="$DAY20_OUTDIR/pass_fail.env"
SUMMARY_TXT="$DAY20_OUTDIR/summary.txt"
DMESG_SCAN_PATTERN='BUG:|Oops:|Call trace:|WARNING:|Kernel panic'

mkdir -p "$DAY20_OUTDIR"

log() {
    echo "[day20][guest] $*"
}

kv_append() {
    key="$1"
    value="$2"
    printf '%s=%s\n' "$key" "$value" >> "$PASS_ENV"
}

summary_append() {
    printf '%s\n' "$*" >> "$SUMMARY_TXT"
}

is_mounted() {
    target="$1"
    grep -q " $target " /proc/mounts 2>/dev/null
}

ensure_mount() {
    target="$1"
    fstype="$2"
    source_dev="$3"

    mkdir -p "$target"
    if ! is_mounted "$target"; then
        mount -t "$fstype" "$source_dev" "$target" 2>/dev/null || true
    fi
}

prepare_basic_fs() {
    ensure_mount /proc proc proc
    ensure_mount /sys sysfs sysfs
    ensure_mount /dev devtmpfs devtmpfs

    if [ -d "$DEBUGFS_ROOT" ]; then
        ensure_mount "$DEBUGFS_ROOT" debugfs debugfs
    fi
    if [ -d "$TRACEFS_ROOT" ]; then
        ensure_mount "$TRACEFS_ROOT" tracefs tracefs
    fi

    mount > "$DAY20_OUTDIR/mount.txt" 2>/dev/null || true
    cat /proc/filesystems > "$DAY20_OUTDIR/filesystems.txt" 2>/dev/null || true

    if is_mounted "$DEBUGFS_ROOT"; then
        kv_append DEBUGFS_OK 1
    else
        kv_append DEBUGFS_OK 0
    fi
}

pick_tracing_dir() {
    if [ -d "$DEBUGFS_ROOT/tracing" ]; then
        TRACING_DIR="$DEBUGFS_ROOT/tracing"
    elif [ -d "$TRACEFS_ROOT" ]; then
        TRACING_DIR="$TRACEFS_ROOT"
    else
        TRACING_DIR=""
    fi

    if [ -n "$TRACING_DIR" ]; then
        printf '%s\n' "$TRACING_DIR" > "$DAY20_OUTDIR/tracing_dir.txt"
        cat "$TRACING_DIR/available_tracers" > "$DAY20_OUTDIR/available_tracers.txt" 2>/dev/null || true
        kv_append TRACING_OK 1
    else
        kv_append TRACING_OK 0
    fi
}

scan_dmesg_tail() {
    dmesg | tail -n 300 > "$DAY20_OUTDIR/dmesg_tail.txt" 2>/dev/null || true
    if grep -Eq "$DMESG_SCAN_PATTERN" "$DAY20_OUTDIR/dmesg_tail.txt" 2>/dev/null; then
        kv_append DMESG_CLEAN 0
    else
        kv_append DMESG_CLEAN 1
    fi
}

capture_meminfo() {
    cat /proc/meminfo > "$DAY20_OUTDIR/meminfo.txt" 2>/dev/null || true
}

capture_modules() {
    cat /proc/modules > "$DAY20_OUTDIR/proc_modules.txt" 2>/dev/null || true
}

emit_file_block() {
    name="$1"
    file="$2"
    echo "__DAY20_FILE_BEGIN__ $name"
    if [ -f "$file" ]; then
        cat "$file"
    fi
    echo "__DAY20_FILE_END__ $name"
}

emit_env_and_files() {
    echo "__DAY20_ENV_BEGIN__"
    [ -f "$PASS_ENV" ] && cat "$PASS_ENV"
    echo "__DAY20_ENV_END__"

    emit_file_block mount.txt "$DAY20_OUTDIR/mount.txt"
    emit_file_block filesystems.txt "$DAY20_OUTDIR/filesystems.txt"
    emit_file_block meminfo.txt "$DAY20_OUTDIR/meminfo.txt"
    emit_file_block proc_modules.txt "$DAY20_OUTDIR/proc_modules.txt"
    emit_file_block available_tracers.txt "$DAY20_OUTDIR/available_tracers.txt"
    emit_file_block smoke.log "$DAY20_OUTDIR/smoke.log"
    emit_file_block snapshot_before.txt "$DAY20_OUTDIR/snapshot_before.txt"
    emit_file_block snapshot_after.txt "$DAY20_OUTDIR/snapshot_after.txt"
    emit_file_block trace.log "$DAY20_OUTDIR/trace.log"
    emit_file_block trace_excerpt.txt "$DAY20_OUTDIR/trace_excerpt.txt"
    emit_file_block stress.log "$DAY20_OUTDIR/stress.log"
    emit_file_block perf_version.txt "$DAY20_OUTDIR/perf_version.txt"
    emit_file_block perf_list.txt "$DAY20_OUTDIR/perf_list.txt"
    emit_file_block perf_stat.txt "$DAY20_OUTDIR/perf_stat.txt"
    emit_file_block dmesg_tail.txt "$DAY20_OUTDIR/dmesg_tail.txt"
    emit_file_block summary.txt "$SUMMARY_TXT"
}
