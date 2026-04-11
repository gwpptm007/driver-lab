#!/bin/sh
set -eu

# Day15 guest_collect.sh
# ----------------------
# 这个脚本运行在 guest（QEMU 里的最小 Linux 系统）中，
# 用来做 Day15 baseline 的“运行态采样”。
#
# 这一版相较前一版，有一个很重要的调整：
#   Day15 现在是自包含目录，不再默认依赖 day13 的 rootfs 或脚本。
#
# 所以它的职责很明确：
# 1. 挂载 proc / sys / dev / debugfs / tracefs（如果可用）
# 2. 采集 meminfo / modules / dmesg / available_tracers
# 3. 检查 tracing / function_graph / perf 的现状
# 4. insmod /demo_regmap.ko，并做一次 snapshot / trigger 冒烟
# 5. 生成 metrics.env，并把关键文件通过 marker 打到串口，便于宿主机提取
#
# 默认用法：
#   /bin/day15_guest_collect.sh
#   /bin/day15_guest_collect.sh /tmp/day15-baseline
#
# 注意：
# Linux 上 tracing 目录有两种常见形态：
# - /sys/kernel/debug/tracing
# - /sys/kernel/tracing
#
# 之前只检查第一种，容易把“tracing 存在但挂在 tracefs”误判成 no。
# 这一版会自动探测两条路径。

OUTDIR="${1:-/tmp/day15-baseline}"
DEBUGFS_ROOT="${DEBUGFS_ROOT:-/sys/kernel/debug}"
TRACEFS_ROOT="${TRACEFS_ROOT:-/sys/kernel/tracing}"
DEMO_DIR="${DEMO_DIR:-/sys/kernel/debug/demo_regmap}"
DEMO_MODULE="${DEMO_MODULE:-/demo_regmap.ko}"
COLLECTOR_VER="${COLLECTOR_VER:-v2}"

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

    echo "__DAY15_FILE_BEGIN__ $name"
    if [ -f "$file" ]; then
        cat "$file"
    fi
    echo "__DAY15_FILE_END__ $name"
}

extract_meminfo() {
    if [ ! -f /proc/meminfo ]; then
        return 0
    fi

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
    # 先尝试 debugfs 形态，再尝试 tracefs 独立挂载形态。
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

# 注意：/sys 是 sysfs，不是普通可写目录。
# 不能像 /tmp 那样随便 mkdir /sys/kernel/tracing，
# 否则会报：Operation not permitted。
#
# 所以这里分两类处理：
# 1) /proc /sys /dev /输出目录：这些是普通挂载点或普通目录，可以 mkdir。
# 2) debugfs / tracefs：只在“挂载点已经存在”时尝试挂载，
#    不主动在 /sys 下面创建新目录。
mkdir -p /proc /sys /dev "$OUTDIR"

# 先挂基础文件系统。
ensure_mount /proc proc proc
ensure_mount /sys sysfs sysfs
ensure_mount /dev devtmpfs devtmpfs

# debugfs 的标准挂载点一般是 /sys/kernel/debug。
# 如果这个目录已经存在，就尝试挂载；不存在就跳过。
if [ -d "$DEBUGFS_ROOT" ]; then
    ensure_mount "$DEBUGFS_ROOT" debugfs debugfs
fi

# tracefs 既可能单独挂在 /sys/kernel/tracing，
# 也可能通过 debugfs 形态暴露在 /sys/kernel/debug/tracing。
# 这里不要强行 mkdir /sys/kernel/tracing，只在目录已存在时尝试挂载。
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
cat /proc/modules > "$OUTDIR/modules.txt" 2>/dev/null || true
dmesg > "$OUTDIR/dmesg.txt" 2>/dev/null || true
dmesg | tail -n 200 > "$OUTDIR/dmesg_tail.txt" 2>/dev/null || true

extract_meminfo
if [ -f /proc/modules ]; then
    modules_loaded_count="$(wc -l < /proc/modules | tr -d ' ')"
fi

if mount | grep -q " on $DEBUGFS_ROOT type debugfs"; then
    debugfs_ok="yes"
fi

if pick_tracing_dir; then
    tracing_ok="yes"
    printf '%s\n' "$tracing_dir_used" > "$OUTDIR/tracing_dir.txt"
    cat "$tracing_dir_used/available_tracers" > "$OUTDIR/available_tracers.txt" 2>/dev/null || true
fi

if [ "$tracing_ok" = "yes" ] && [ -f "$tracing_dir_used/available_tracers" ] && grep -wq function_graph "$tracing_dir_used/available_tracers"; then
    function_graph_ok="yes"

    # 最小 trace 冒烟：
    # 这里只验证 tracer 可切换，不追求完整 function_graph 输出归档。
    if echo 0 > "$tracing_dir_used/tracing_on" 2>/dev/null \
       && echo function_graph > "$tracing_dir_used/current_tracer" 2>/dev/null \
       && echo nop > "$tracing_dir_used/current_tracer" 2>/dev/null; then
        trace_smoke_ok="yes"
    fi
fi

if command -v perf >/dev/null 2>&1; then
    perf_bin_ok="yes"
    if perf stat true > "$OUTDIR/perf_stat.txt" 2>&1; then
        perf_smoke_ok="yes"
    else
        perf_smoke_ok="no"
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

echo "[ OK  ] day15 guest collection done: $OUTDIR"
echo "[ INFO] tracing dir used: ${tracing_dir_used:-none}"
echo "[ INFO] next step: cat $OUTDIR/metrics.env"

echo "__DAY15_ENV_BEGIN__"
cat "$OUTDIR/metrics.env"
echo "__DAY15_ENV_END__"

emit_file_block mount.txt "$OUTDIR/mount.txt"
emit_file_block filesystems.txt "$OUTDIR/filesystems.txt"
emit_file_block meminfo.txt "$OUTDIR/meminfo.txt"
emit_file_block modules.txt "$OUTDIR/modules.txt"
emit_file_block available_tracers.txt "$OUTDIR/available_tracers.txt"
emit_file_block dmesg_tail.txt "$OUTDIR/dmesg_tail_after_action.txt"
emit_file_block snapshot.txt "$OUTDIR/snapshot.txt"

exit 0
