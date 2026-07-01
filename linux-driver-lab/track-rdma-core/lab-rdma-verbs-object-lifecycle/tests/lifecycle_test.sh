#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY="${ROOT_DIR}/build/rdma-lifecycle"
PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0

pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    printf 'PASS: %s\n' "$1"
}

fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    printf 'FAIL: %s\n' "$1" >&2
}

skip() {
    SKIP_COUNT=$((SKIP_COUNT + 1))
    printf 'SKIP: %s\n' "$1"
}

expect_success_contains() {
    # 验证命令成功，并检查稳定输出中是否包含目标字段。
    local name="$1"
    local expected="$2"
    shift 2
    local output

    if ! output="$("$@" 2>&1)"; then
        fail "${name}: command returned non-zero"
        printf '%s\n' "${output}" >&2
        return
    fi
    if grep -Fq -- "${expected}" <<<"${output}"; then
        pass "${name}"
    else
        fail "${name}: missing '${expected}'"
        printf '%s\n' "${output}" >&2
    fi
}

expect_failure_contains() {
    # 错误场景必须同时满足：退出码非零，并输出可诊断的 error 字段。
    local name="$1"
    local expected="$2"
    shift 2
    local output

    if output="$("$@" 2>&1)"; then
        fail "${name}: command unexpectedly succeeded"
        printf '%s\n' "${output}" >&2
        return
    fi
    if grep -Fq -- "${expected}" <<<"${output}"; then
        pass "${name}"
    else
        fail "${name}: missing '${expected}'"
        printf '%s\n' "${output}" >&2
    fi
}

if [[ ! -x "${BINARY}" ]]; then
    printf 'FAIL: binary not found: %s\n' "${BINARY}" >&2
    exit 1
fi

# 第一组是纯 CLI 测试，即使机器没有 RDMA device 也应执行。
expect_success_contains "help" "usage: rdma-lifecycle" "${BINARY}" --help
expect_failure_contains "unknown option" "error=invalid_arguments" "${BINARY}" --unknown
expect_failure_contains "invalid port" "error=invalid_port" "${BINARY}" --port 0
expect_success_contains "list devices" "device_count=" "${BINARY}" --list
expect_failure_contains "missing device" "error=device_not_found" \
    "${BINARY}" --device definitely-not-an-rdma-device

# 第二组是真实集成测试：从 --list 输出中选择第一个 provider-visible device。
device_name="$(${BINARY} --list 2>/dev/null | sed -n 's/^device\[[0-9][0-9]*\]=//p' | head -n 1)"
if [[ -n "${device_name}" ]]; then
    lifecycle_output="$(${BINARY} --device "${device_name}" --port 1 2>&1)"
    lifecycle_rc=$?
    # 不只看退出码，还验证 QP 初始状态、逆序清理和最终结果三项语义。
    if [[ ${lifecycle_rc} -eq 0 ]] &&
       grep -Fq 'qp_state=RESET' <<<"${lifecycle_output}" &&
       grep -Fq 'cleanup=complete' <<<"${lifecycle_output}" &&
       grep -Fq 'result=pass' <<<"${lifecycle_output}"; then
        pass "real verbs lifecycle (${device_name})"
    else
        fail "real verbs lifecycle (${device_name})"
        printf '%s\n' "${lifecycle_output}" >&2
    fi
else
    skip "real verbs lifecycle: no provider-visible device"
fi

printf 'SUMMARY: pass=%d fail=%d skip=%d\n' \
    "${PASS_COUNT}" "${FAIL_COUNT}" "${SKIP_COUNT}"

[[ ${FAIL_COUNT} -eq 0 ]]
