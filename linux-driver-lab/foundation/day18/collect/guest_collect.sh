#!/bin/sh
set -eu

# Day18 guest_collect.sh
# ----------------------
# Day18 现在已经把 perf 正式纳入“可集成、可验证”的主线，因此这份 guest 采样脚本也同步增强：
#
# 1. 继续保留 tracing/function_graph 的最小冒烟；
# 2. 如果 guest 里存在 perf，则记录：
#      - perf --version
#      - perf list software（前 80 行）
#      - perf stat -e task-clock -- /bin/true
#      - /etc/day18_perf_manifest.txt（如果 build.sh 打进来了）
# 3. 让 host_collect.sh 回收这些文本后，能非常直接地判断：
#      - perf 是否真的在 guest 中可执行
#      - perf 是“文件存在但缺库”，还是完整可跑
#
# 默认用法：
#   /bin/day18_guest_collect.sh
#   /bin/day18_guest_collect.sh /tmp/day18-baseline

OUTDIR="${1:-/tmp/day18-baseline}"
DEBUGFS_ROOT="${DEBUGFS_ROOT:-/sys/kernel/debug}"
TRACEFS_ROOT="${TRACEFS_ROOT:-/sys/kernel/tracing}"
DEMO_DIR="${DEMO_DIR:-/sys/kernel/debug/demo_regmap}"
DEMO_MODULE="${DEMO_MODULE:-/demo_regmap.ko}"
COLLECTOR_VER="${COLLECTOR_VER:-v4}"

boot_ok="yes"
debugfs_ok="no"
tracing_ok="no"
function_graph_ok="no"
trace_smoke_ok="no"
perf_bin_ok="no"
perf_smoke_ok="pending"
insmod_ok="no"
snapshot_ok="no"
trigger_ok="no"
dmesg_warn="no"
remarks=""
modules_loaded_count="0"
memtotal_kib="0"
memfree_kib="0"
memavailable_kib="0"
slab_kib="0"
sreclaimable_kib="0"
sunreclaim_kib="0"
kernelstack_kib="0"
pagetables_kib="0"
tracing_dir_used=""

mkdir -p "$OUTDIR"

ensure_mount() {
    target="$1"
    fstype="$2"
    source_dev="$3"

    if ! mount | grep -q " on $target type $fstype"; then
        mount -t "$fstype" "$source_dev" "$target" 2>/dev/null || true
    fi
}

emit_file_block() {
    name="$1"
    file="$2"

    echo "__DAY18_FILE_BEGIN__ $name"
    if [ -f "$file" ]; then
        cat "$file"
    fi
    echo "__DAY18_FILE_END__ $name"
}

extract_meminfo() {
    [ -f /proc/meminfo ] || return 0

    while IFS=' ' read -r key value unit; do
        case "$key" in
            MemTotal:) memtotal_kib="$value" ;;
            MemFree:) memfree_kib="$value" ;;
            MemAvailable:) memavailable_kib="$value" ;;
            Slab:) slab_kib="$value" ;;
            SReclaimable:) sreclaimable_kib="$value" ;;
            SUnreclaim:) sunreclaim_kib="$value" ;;
            KernelStack:) kernelstack_kib="$value" ;;
            PageTables:) pagetables_kib="$value" ;;
        esac
    done < /proc/meminfo
}

pick_tracing_dir() {
    if [ -d "$DEBUGFS_ROOT/tracing" ]; then
        tracing_dir_used="$DEBUGFS_ROOT/tracing"
        return 0
    fi
    if [ -d "$TRACEFS_ROOT" ]; then
        tracing_dir_used="$TRACEFS_ROOT"
        return 0
    fi

    tracing_dir_used=""
    return 1
}

mkdir -p /proc /sys /dev "$OUTDIR"
ensure_mount /proc proc proc
ensure_mount /sys sysfs sysfs
ensure_mount /dev devtmpfs devtmpfs

if [ -d "$DEBUGFS_ROOT" ]; then
    ensure_mount "$DEBUGFS_ROOT" debugfs debugfs
fi
if [ -d "$TRACEFS_ROOT" ]; then
    ensure_mount "$TRACEFS_ROOT" tracefs tracefs
fi
sleep 1

uname -a > "$OUTDIR/uname.txt" 2>/dev/null || true
cat /proc/cmdline > "$OUTDIR/cmdline.txt" 2>/dev/null || true
cut -d' ' -f1 /proc/uptime > "$OUTDIR/uptime_seconds.txt" 2>/dev/null || true
mount > "$OUTDIR/mount.txt" 2>/dev/null || true
cat /proc/filesystems > "$OUTDIR/filesystems.txt" 2>/dev/null || true
cat /proc/meminfo > "$OUTDIR/meminfo.txt" 2>/dev/null || true
dmesg > "$OUTDIR/dmesg.txt" 2>/dev/null || true
dmesg | tail -n 200 > "$OUTDIR/dmesg_tail.txt" 2>/dev/null || true
[ -f /etc/day18_perf_manifest.txt ] && cp /etc/day18_perf_manifest.txt "$OUTDIR/perf_manifest.txt" || true

extract_meminfo

if mount | grep -q " on $DEBUGFS_ROOT type debugfs"; then
    debugfs_ok="yes"
fi

if pick_tracing_dir; then
    tracing_ok="yes"
    printf '%s
' "$tracing_dir_used" > "$OUTDIR/tracing_dir.txt"
    cat "$tracing_dir_used/available_tracers" > "$OUTDIR/available_tracers.txt" 2>/dev/null || true
fi

if [ "$tracing_ok" = "yes" ] && [ -f "$tracing_dir_used/available_tracers" ] && grep -wq function_graph "$tracing_dir_used/available_tracers"; then
    function_graph_ok="yes"
    if echo 0 > "$tracing_dir_used/tracing_on" 2>/dev/null \
       && echo function_graph > "$tracing_dir_used/current_tracer" 2>/dev/null \
       && echo nop > "$tracing_dir_used/current_tracer" 2>/dev/null; then
        trace_smoke_ok="yes"
    fi
fi

if command -v perf >/dev/null 2>&1; then
    if perf --version > "$OUTDIR/perf_version.txt" 2>&1; then
        perf_bin_ok="yes"
        perf list software | head -n 80 > "$OUTDIR/perf_list.txt" 2>&1 || true
        # 这里显式使用 /bin/true，而不是裸 true。
        # 原因：最小 rootfs 中 PATH / applet 链接是否齐全，容易让 perf 的 workload 失败。
        # Day18 最终版已经在 build.sh 中默认创建 /bin/true -> /bin/busybox，
        # 因此这里固定写成 /bin/true，能让 perf smoke 更稳定。
        if perf stat -e task-clock -- /bin/true > "$OUTDIR/perf_stat.txt" 2>&1; then
            perf_smoke_ok="yes"
        else
            perf_smoke_ok="no"
            remarks="${remarks}perf_stat_failed;"
        fi
    else
        perf_bin_ok="no"
        perf_smoke_ok="no"
        remarks="${remarks}perf_exec_failed;"
    fi
else
    perf_bin_ok="no"
    perf_smoke_ok="pending"
fi

if grep -q '^demo_regmap ' /proc/modules 2>/dev/null; then
    insmod_ok="yes"
else
    if [ -f "$DEMO_MODULE" ]; then
        if insmod "$DEMO_MODULE" > "$OUTDIR/insmod_stdout.txt" 2> "$OUTDIR/insmod_stderr.txt"; then
            insmod_ok="yes"
        else
            insmod_ok="no"
            remarks="${remarks}insmod_failed;"
        fi
    else
        insmod_ok="no"
        remarks="${remarks}demo_module_missing;"
    fi
fi

if [ -d "$DEMO_DIR" ] && [ -f "$DEMO_DIR/snapshot" ]; then
    if cat "$DEMO_DIR/snapshot" > "$OUTDIR/snapshot.txt" 2>/dev/null; then
        snapshot_ok="yes"
    fi
fi

if [ -d "$DEMO_DIR" ] && [ -f "$DEMO_DIR/trigger" ]; then
    if echo 1 > "$DEMO_DIR/trigger" 2> "$OUTDIR/trigger_stderr.txt"; then
        trigger_ok="yes"
    else
        trigger_ok="no"
        remarks="${remarks}trigger_failed;"
    fi
fi

dmesg | tail -n 200 > "$OUTDIR/dmesg_tail_after_action.txt" 2>/dev/null || true
if grep -Eq 'BUG:|Oops:|Call trace:|WARNING:' "$OUTDIR/dmesg_tail_after_action.txt" 2>/dev/null; then
    dmesg_warn="yes"
fi

cat /proc/modules > "$OUTDIR/modules.txt" 2>/dev/null || true
if [ -f /proc/modules ]; then
    modules_loaded_count="$(wc -l < /proc/modules | tr -d ' ')"
fi

cat > "$OUTDIR/metrics.env" <<EOF2
collector_ver=$COLLECTOR_VER
boot_ok=$boot_ok
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
insmod_ok=$insmod_ok
snapshot_ok=$snapshot_ok
trigger_ok=$trigger_ok
dmesg_warn=$dmesg_warn
remarks=$remarks
EOF2

echo "[ OK  ] day18 guest collection done: $OUTDIR"
echo "[ INFO] tracing dir used: ${tracing_dir_used:-none}"
echo "[ INFO] next step: cat $OUTDIR/metrics.env"

echo "__DAY18_ENV_BEGIN__"
cat "$OUTDIR/metrics.env"
echo "__DAY18_ENV_END__"

emit_file_block mount.txt "$OUTDIR/mount.txt"
emit_file_block filesystems.txt "$OUTDIR/filesystems.txt"
emit_file_block meminfo.txt "$OUTDIR/meminfo.txt"
emit_file_block modules.txt "$OUTDIR/modules.txt"
emit_file_block available_tracers.txt "$OUTDIR/available_tracers.txt"
emit_file_block dmesg_tail.txt "$OUTDIR/dmesg_tail_after_action.txt"
emit_file_block snapshot.txt "$OUTDIR/snapshot.txt"
emit_file_block perf_version.txt "$OUTDIR/perf_version.txt"
emit_file_block perf_list.txt "$OUTDIR/perf_list.txt"
emit_file_block perf_stat.txt "$OUTDIR/perf_stat.txt"
emit_file_block perf_manifest.txt "$OUTDIR/perf_manifest.txt"
emit_file_block tracing_dir.txt "$OUTDIR/tracing_dir.txt"

exit 0
