#!/usr/bin/env bash
set -euo pipefail

# Day17 run_profile_collect.sh
# -----------------------------
# 这个脚本把“单个 profile 的完整测试 + 证据链采集”收口成一个入口。
#
# 它不仅会跑 apply_config/build/host_collect，还会在本轮 records 目录里补充：
#   1. 最终生效的 .config
#   2. 内核 Image / rootfs.img / demo_regmap.ko 的大小与 sha256
#   3. modules.order / modules.builtin
#   4. build 侧 perf bundle manifest
#   5. 一个 artifact_evidence.env，供 compare_results.py 自动汇总证据链
#
# 这样下一步做 baseline / round1 / round2b 对比时，不再只看 boot_ms/image_kib，
# 而是能回答“fragment 到底有没有落到最终产物上”。

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
RECORDS_DIR="$SCRIPT_DIR/records"
PROFILE_ARG="${1:-${PROFILE:-baseline}}"
PROFILE="$PROFILE_ARG"
PERF_REQUIRED="${PERF_REQUIRED:-yes}"
PERF_MODE="${PERF_MODE:-auto}"
QEMU_AUTO_BOOT="${QEMU_AUTO_BOOT:-no}"
SCENARIO_ID="${SCENARIO_ID:-}"
CLEAN_TMP_OUTPUT="${CLEAN_TMP_OUTPUT:-no}"
ARCH_NAME="${ARCH_NAME:-arm64}"
KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../kernel-src/linux-5.15.10}"
KERNEL_SRC="${KERNEL_SRC:-$KERNEL_DIR/src}"
KERNEL_OUT="${KERNEL_OUT:-$KERNEL_DIR/build/arm64}"
KERNEL_IMG="${KERNEL_IMG:-$KERNEL_DIR/output/arm64/Image}"
ROOTFS_IMG="${ROOTFS_IMG:-$SCRIPT_DIR/rootfs.img}"
MODULE_SEARCH_DIR="${MODULE_SEARCH_DIR:-$SCRIPT_DIR}"
PERF_MANIFEST="${PERF_MANIFEST:-$SCRIPT_DIR/output/perf/perf_bundle_manifest.txt}"

fail() {
    echo "[ERROR] $*" >&2
    exit 1
}

info() {
    echo "[INFO] $*"
}

sha256_file() {
    local path="$1"
    [ -f "$path" ] || return 0
    sha256sum "$path" | awk '{print $1}'
}

copy_if_exists() {
    local src="$1"
    local dst="$2"
    if [ -f "$src" ]; then
        mkdir -p "$(dirname "$dst")"
        cp -L "$src" "$dst"
    fi
}

build_fragment_chain() {
    case "$PROFILE" in
        baseline)
            printf '%s\n' "$SCRIPT_DIR/config/trace_baseline.fragment"
            ;;
        round1)
            printf '%s\n' \
                "$SCRIPT_DIR/config/trace_baseline.fragment" \
                "$SCRIPT_DIR/config/trim_round1.fragment"
            ;;
        round2b)
            printf '%s\n' \
                "$SCRIPT_DIR/config/trace_baseline.fragment" \
                "$SCRIPT_DIR/config/trim_round1.fragment" \
                "$SCRIPT_DIR/config/trim_round2b.fragment"
            ;;
        *)
            fail "unsupported profile: $PROFILE"
            ;;
    esac
}

capture_build_evidence() {
    local record_dir="$1"
    local evidence_dir="$record_dir/build_evidence"
    local config_copy="$evidence_dir/kernel.config"
    local image_sha=""
    local rootfs_sha=""
    local module_sha=""
    local config_sha=""
    local image_bytes=""
    local rootfs_bytes=""
    local module_bytes=""
    local kernel_release="unknown"
    local line_count="0"
    local fragment

    mkdir -p "$evidence_dir/fragments"

    while IFS= read -r fragment; do
        [ -n "$fragment" ] || continue
        if [ -f "$fragment" ]; then
            cp -L "$fragment" "$evidence_dir/fragments/$(basename "$fragment")"
            printf '%s\n' "$fragment" >> "$evidence_dir/applied_fragments.txt"
        fi
    done < <(build_fragment_chain)

    if [ -f "$KERNEL_OUT/.config" ]; then
        cp -L "$KERNEL_OUT/.config" "$config_copy"
        config_sha=$(sha256_file "$config_copy")
        grep -E '^(CONFIG_(DEBUG_FS|TRACEFS_FS|TRACING|TRACEPOINTS|FTRACE|FUNCTION_TRACER|FUNCTION_GRAPH_TRACER|DYNAMIC_FTRACE|KALLSYMS|KALLSYMS_ALL|PERF_EVENTS|HW_PERF_EVENTS|MODULES|BLK_DEV_INITRD|PCI|NET|SCSI|SOUND|USB|RCU_EXPERT|IKCONFIG|IKCONFIG_PROC|HID|INPUT_|FB|I2C_|NVME|BTRFS_FS)=|# CONFIG_(USB_|HID|INPUT_|FB|PCI_|NET|SCSI|SOUND|I2C_|NVME|BTRFS_FS).+ is not set$)' "$config_copy" > "$evidence_dir/kernel.config.focus.txt" || true
        line_count=$(wc -l < "$config_copy" | tr -d ' ')
    fi

    if [ -f "$KERNEL_SRC/Makefile" ] && [ -d "$KERNEL_OUT" ]; then
        kernel_release=$(make -s -C "$KERNEL_SRC" O="$KERNEL_OUT" kernelrelease 2>/dev/null || echo unknown)
    fi

    if [ -f "$KERNEL_IMG" ]; then
        copy_if_exists "$KERNEL_IMG" "$evidence_dir/Image"
        image_sha=$(sha256_file "$KERNEL_IMG")
        image_bytes=$(stat -c '%s' "$KERNEL_IMG")
        printf '%s  %s\n' "$image_sha" "$KERNEL_IMG" > "$evidence_dir/Image.sha256"
        file "$KERNEL_IMG" > "$evidence_dir/Image.file.txt" 2>/dev/null || true
    fi

    if [ -f "$ROOTFS_IMG" ]; then
        copy_if_exists "$ROOTFS_IMG" "$evidence_dir/rootfs.img"
        rootfs_sha=$(sha256_file "$ROOTFS_IMG")
        rootfs_bytes=$(stat -c '%s' "$ROOTFS_IMG")
        printf '%s  %s\n' "$rootfs_sha" "$ROOTFS_IMG" > "$evidence_dir/rootfs.img.sha256"
        file "$ROOTFS_IMG" > "$evidence_dir/rootfs.img.file.txt" 2>/dev/null || true
    fi

    if [ -f "$MODULE_SEARCH_DIR/demo_regmap.ko" ]; then
        copy_if_exists "$MODULE_SEARCH_DIR/demo_regmap.ko" "$evidence_dir/demo_regmap.ko"
        module_sha=$(sha256_file "$MODULE_SEARCH_DIR/demo_regmap.ko")
        module_bytes=$(stat -c '%s' "$MODULE_SEARCH_DIR/demo_regmap.ko")
        printf '%s  %s\n' "$module_sha" "$MODULE_SEARCH_DIR/demo_regmap.ko" > "$evidence_dir/demo_regmap.ko.sha256"
        file "$MODULE_SEARCH_DIR/demo_regmap.ko" > "$evidence_dir/demo_regmap.ko.file.txt" 2>/dev/null || true
    fi

    copy_if_exists "$KERNEL_OUT/modules.order" "$evidence_dir/modules.order"
    copy_if_exists "$KERNEL_OUT/modules.builtin" "$evidence_dir/modules.builtin"
    copy_if_exists "$KERNEL_OUT/modules.builtin.modinfo" "$evidence_dir/modules.builtin.modinfo"
    copy_if_exists "$PERF_MANIFEST" "$evidence_dir/perf_bundle_manifest.txt"

    cat > "$evidence_dir/artifact_evidence.env" <<EOF
profile=$PROFILE
scenario_id=$SCENARIO_ID
kernel_release=$kernel_release
kernel_config_sha256=$config_sha
kernel_config_line_count=$line_count
kernel_image_sha256=$image_sha
kernel_image_bytes=$image_bytes
rootfs_img_sha256=$rootfs_sha
rootfs_img_bytes=$rootfs_bytes
module_demo_sha256=$module_sha
module_demo_bytes=$module_bytes
kernel_out=$KERNEL_OUT
kernel_img=$KERNEL_IMG
rootfs_img=$ROOTFS_IMG
perf_manifest_present=$( [ -f "$PERF_MANIFEST" ] && echo yes || echo no )
EOF

    info "build evidence      : $evidence_dir"
}

case "$PROFILE" in
    baseline|round1|round2b)
        ;;
    *)
        fail "unsupported profile: $PROFILE (expected: baseline/round1/round2b)"
        ;;
esac

if [ -z "$SCENARIO_ID" ]; then
    SCENARIO_ID="day17-${PROFILE}-arm64-virt"
fi

mkdir -p "$RECORDS_DIR"
LAST_PTR="$RECORDS_DIR/LAST_${PROFILE}.txt"
BEFORE_SNAPSHOT=$(mktemp)
AFTER_SNAPSHOT=$(mktemp)
trap 'rm -f "$BEFORE_SNAPSHOT" "$AFTER_SNAPSHOT"' EXIT

find "$RECORDS_DIR" -mindepth 1 -maxdepth 1 -type d | sort > "$BEFORE_SNAPSHOT"

if [ "$CLEAN_TMP_OUTPUT" = "yes" ]; then
    info "cleaning previous day17 build outputs for a fresh run"
    rm -rf "$SCRIPT_DIR/rootfs" "$SCRIPT_DIR/rootfs.img" "$SCRIPT_DIR/virt-day17.dtb"
fi

info "profile            : $PROFILE"
info "scenario_id        : $SCENARIO_ID"
info "perf_required      : $PERF_REQUIRED"
info "perf_mode          : $PERF_MODE"
info "qemu_auto_boot     : $QEMU_AUTO_BOOT"

PROFILE="$PROFILE" ./apply_config.sh
PROFILE="$PROFILE" PERF_REQUIRED="$PERF_REQUIRED" PERF_MODE="$PERF_MODE" QEMU_AUTO_BOOT="$QEMU_AUTO_BOOT" ./build.sh
(
    cd "$SCRIPT_DIR/collect"
    SCENARIO_ID="$SCENARIO_ID" ./host_collect.sh
)

find "$RECORDS_DIR" -mindepth 1 -maxdepth 1 -type d | sort > "$AFTER_SNAPSHOT"
LATEST_RECORD=$(comm -13 "$BEFORE_SNAPSHOT" "$AFTER_SNAPSHOT" | tail -n 1 || true)

if [ -z "$LATEST_RECORD" ]; then
    LATEST_RECORD=$(find "$RECORDS_DIR" -mindepth 1 -maxdepth 1 -type d -name "*-${SCENARIO_ID}" | sort | tail -n 1 || true)
fi

[ -n "$LATEST_RECORD" ] || fail "unable to determine latest record directory for scenario $SCENARIO_ID"
[ -f "$LATEST_RECORD/metrics.env" ] || fail "metrics.env not found in $LATEST_RECORD"

capture_build_evidence "$LATEST_RECORD"

echo "$LATEST_RECORD" > "$LAST_PTR"
info "latest record       : $LATEST_RECORD"
info "last pointer        : $LAST_PTR"
info "done"
