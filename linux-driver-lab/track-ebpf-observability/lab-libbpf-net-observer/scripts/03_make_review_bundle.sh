#!/usr/bin/env bash
set -euo pipefail

LAB_NAME="lab-libbpf-net-observer"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RECORD_ROOT="${LAB_DIR}/records"
REPORT_DIR="${LAB_DIR}/reports"

last_record_dir() {
    if [[ -f "${RECORD_ROOT}/.last_record_dir" ]]; then
        cat "${RECORD_ROOT}/.last_record_dir"
    else
        echo "${RECORD_ROOT}/unknown"
    fi
}

has_file() { [[ -s "${RD}/$1" ]]; }
has_build_ok() {
    grep -q "BUILD_OK=1" "${RD}/BUILD.log" 2>/dev/null
}
has_events() {
    grep -qE "@[^:]+:[[:space:]]*[1-9]" "${RD}/OBSERVER_RUN.log" 2>/dev/null || \
    grep -qE "Event[[:space:]]+Count" "${RD}/OBSERVER_RUN.log" 2>/dev/null
}

main() {
    RD="$(last_record_dir)"
    local out="${RD}/REVIEW_BUNDLE.md"

    local PASS_ENV=NO PASS_BUILD=NO PASS_RUN=NO EVENTS=NO

    has_file ENV_CHECK.txt && PASS_ENV=YES
    has_file BUILD.log && has_build_ok && PASS_BUILD=YES
    has_file OBSERVER_RUN.log && has_events && EVENTS=YES
    local rc_val
    rc_val=$(grep "^RC=" "${RD}/OBSERVER_RUN.log" 2>/dev/null | tail -1 | cut -d= -f2)
    [[ "${rc_val}" == "0" ]] && PASS_RUN=YES

    {
        echo "# REVIEW_BUNDLE - ${LAB_NAME}"
        echo
        echo "- record_dir: \`${RD}\`"
        echo "- date: $(date -Iseconds)"
        echo
        echo "## 文件状态"
        echo
        echo "| file | status |"
        echo "|---|---|"
        for f in ENV_CHECK.txt BUILD.log OBSERVER_RUN.log; do
            if has_file "$f"; then echo "| ${f} | DONE |"
            else echo "| ${f} | MISSING |"; fi
        done
        echo
        echo "## 判定"
        echo
        echo "| item | result |"
        echo "|---|---|"
        echo "| PASS_ENV   | ${PASS_ENV} |"
        echo "| PASS_BUILD | ${PASS_BUILD} |"
        echo "| PASS_RUN   | ${PASS_RUN} |"
        echo "| EVENTS_OBSERVED | ${EVENTS} |"
        echo
        echo "## bpftrace vs libbpf 演进"
        echo
        echo "| 维度 | Phase 3 (bpftrace) | Phase 4 (libbpf) |"
        echo "|---|---|---|"
        echo "| 语言 | bpftrace 脚本 (~50 行) | C 程序 (BPF + userspace, ~300 行) |"
        echo "| 编译 | 无需编译，bpftrace 解释执行 | clang + bpftool + gcc 三阶段编译 |"
        echo "| 输出 | terminal printf 打印 maps | ringbuf 事件流 → userspace 格式化 |"
        echo "| 可移植 | 目标机器需安装 bpftrace | 单一二进制，无外部运行时依赖 |"
        echo "| CO-RE | 不支持 | 支持 (vmlinux.h + BTF) |"
        echo "| 部署 | 源码拷贝 | 单文件二进制 (可打包 .deb/.rpm) |"
        echo
        echo "## 结论建议"
        echo
        if [[ "${PASS_ENV}" == "YES" && "${PASS_BUILD}" == "YES" && "${PASS_RUN}" == "YES" && "${EVENTS}" == "YES" ]]; then
            echo "PASS_LIBBPF_OBSERVER"
        else
            echo "NEED_RETEST_OR_FIX"
        fi
        echo
        echo "## 说明"
        echo
        echo "Phase 4 的最低验收: 编译通过 + 运行正常 + 捕获到事件。"
        echo "这是 track 从脚本级 → 编译级工具的关键转折。"
    } | tee "${out}"

    echo "REVIEW_BUNDLE=${out}"
}

main "$@"
