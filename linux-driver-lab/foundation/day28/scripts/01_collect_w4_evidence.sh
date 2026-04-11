#!/usr/bin/env bash
# 汇总 day22~day27 的真实 records，生成一份轻量快照。
#
# 这一步不修改原始 records，只做复制和索引，方便 day28 统一生成总结文档。

set -euo pipefail
source "$(dirname "$0")/common.sh"

ensure_dir "${SNAPSHOT_DIR}"
rm -rf "${SNAPSHOT_DIR:?}"/*

for day in day22 day23 day24 day25 day26 day27; do
    src_dir="${LAB_ROOT}/${day}/records"
    if [[ ! -d "${src_dir}" ]]; then
        warn "跳过 ${day}：records 目录不存在"
        continue
    fi

    day_out="${SNAPSHOT_DIR}/${day}"
    ensure_dir "${day_out}"

    # 只复制每个 day 的第一个本地 run 目录和 README，避免 day28 快照过大。
    cp -f "${src_dir}/README.md" "${day_out}/" 2>/dev/null || true

    first_run="$(find "${src_dir}" -mindepth 1 -maxdepth 1 -type d | sort | head -n 1 || true)"
    if [[ -n "${first_run}" ]]; then
        cp -a "${first_run}" "${day_out}/"
    else
        warn "${day} 没有找到实际 run 目录"
    fi

done

log "W4 证据快照已生成：${SNAPSHOT_DIR}"
