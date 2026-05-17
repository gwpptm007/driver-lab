#!/usr/bin/env bash
#============================================================
# common.sh — lab-bpftrace-netdev-observe 公共函数库
#
# 功能：
#   - 定义环境变量默认值（观测网卡、时长）
#   - 提供记录目录创建、root 检查、XDP状态检查、bpftrace 运行等公共函数
#   - 被 scripts/ 下所有脚本 source 使用
#
# 主要环境变量：
#   BPFTRACE_IFACE           : 观测的目标网卡（默认 ens192）
#   BPFTRACE_MANAGEMENT_IFACE: 管理网口（默认 ens33）
#   BPFTRACE_DURATION        : bpftrace 运行时间（默认 10 秒）
#   EBPF_CONFIRM_XDP_OFF     : 确认关闭 XDP（需设为 YES）
#============================================================

set -euo pipefail

#------------------------------
# 路径变量
#------------------------------
LAB_NAME="lab-bpftrace-netdev-observe"                    # 实验名称
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"  # scripts/ 目录
LAB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"                  # lab-bpftrace-netdev-observe/ 根目录
RECORDS_DIR="${LAB_DIR}/records"                            # 测试记录目录
LAST_RECORD_FILE="${RECORDS_DIR}/.last_record_dir"          # 上一次记录目录的路径文件

#------------------------------
# 环境变量默认值
#------------------------------
: "${BPFTRACE_IFACE:=ens192}"                # 观测的目标网卡（实验网卡）
: "${BPFTRACE_MANAGEMENT_IFACE:=ens33}"     # 管理网口（SSH/管理用途）
: "${BPFTRACE_DURATION:=10}"                # bpftrace 单次运行时间（秒）

mkdir -p "${RECORDS_DIR}"

#------------------------------
# new_record_dir — 创建新的带时间戳记录目录
#
# 创建 records/YYYYMMDD-HHMMSS-bpftrace-netdev-observe/ 目录并返回路径。
# 每一次完整实验运行对应一个记录目录。
#------------------------------
new_record_dir() {
    local ts dir
    ts="$(date +%Y%m%d-%H%M%S)"
    dir="${RECORDS_DIR}/${ts}-bpftrace-netdev-observe"
    mkdir -p "${dir}"
    printf '%s\n' "${dir}" > "${LAST_RECORD_FILE}"   # 记录到 .last_record_dir
    printf '%s\n' "${dir}"
}

#------------------------------
# get_record_dir — 返回当前记录目录
#
# 参数：$1 可选，指定目录路径
# 优先级：
#   1. 如果传入参数且非空，使用传入的目录
#   2. 如果 .last_record_dir 存在且非空，使用其中的路径
#   3. 否则创建新的时间戳目录
#------------------------------
get_record_dir() {
    if [[ $# -gt 0 && -n "${1:-}" ]]; then
        mkdir -p "$1"
        printf '%s\n' "$1" > "${LAST_RECORD_FILE}"
        printf '%s\n' "$1"
        return 0
    fi
    if [[ -f "${LAST_RECORD_FILE}" ]]; then
        local dir
        dir="$(cat "${LAST_RECORD_FILE}")"
        if [[ -n "${dir}" ]]; then
            mkdir -p "${dir}"
            printf '%s\n' "${dir}"
            return 0
        fi
    fi
    new_record_dir
}

#------------------------------
# require_root_for_bpftrace — 确保以 root 身份运行
#
# bpftrace 需要 CAP_BPF/CAP_SYS_ADMIN 权限，通常需要 sudo。
# 如果不是 root，则报错退出。
#------------------------------
require_root_for_bpftrace() {
    if [[ "${EUID}" -ne 0 ]]; then
        echo "ERROR: this script needs root because bpftrace normally needs CAP_BPF/CAP_SYS_ADMIN." >&2
        echo "Run with sudo." >&2
        exit 1
    fi
}

#------------------------------
# has_xdp_attached — 检查目标网卡是否有 XDP 程序附加
#
# 返回：0（有XDP）或 1（无XDP）
#------------------------------
has_xdp_attached() {
    ip -d link show "${BPFTRACE_IFACE}" 2>/dev/null | grep -q ' xdp\|prog/xdp'
}

#------------------------------
# run_bt — 运行 bpftrace 脚本并记录输出
#
# 参数：
#   $1 — probe_file  bpftrace 脚本路径（.bt 文件）
#   $2 — log_file    输出日志文件路径
#   $3 — duration   运行时间（秒），默认 BPFTRACE_DURATION
#
# 功能：
#   1. 清空日志文件，写入探针元信息（路径、时长、网卡、内核版本、日期）
#   2. 执行 bpftrace 并将输出同时 tee 到终端和日志文件
#   3. timeout 退出码 124 视为正常（观察超时不是错误）
#   4. 其他非零退出码记录到日志
#------------------------------
run_bt() {
    local probe_file="$1"
    local log_file="$2"
    local duration="${3:-${BPFTRACE_DURATION}}"

    # 清空日志文件并写入探针元信息
    : > "${log_file}"
    {
        echo "probe_file=${probe_file}"
        echo "duration=${duration}"
        echo "iface_hint=${BPFTRACE_IFACE}"
        echo "kernel=$(uname -r)"
        echo "date=$(date -Is)"
        echo "note=tracepoint-first, no BEGIN/END blocks"
        echo "--- bpftrace output ---"
    } | tee -a "${log_file}"

    # 运行 bpftrace，timeout 控制运行时长
    set +e
    timeout "${duration}" bpftrace "${probe_file}" 2>&1 | tee -a "${log_file}"
    local rc=${PIPESTATUS[0]}
    set -e

    if [[ "${rc}" -eq 124 ]]; then
        # timeout 退出码 124：运行时长到了，不是错误
        echo "BPFTRACE_TIMEOUT_RC=124" | tee -a "${log_file}"
        return 0
    fi
    if [[ "${rc}" -ne 0 ]]; then
        # 其他错误码记录到日志
        echo "BPFTRACE_RC=${rc}" | tee -a "${log_file}"
        return "${rc}"
    fi
    echo "BPFTRACE_RC=0" | tee -a "${log_file}"
}

#------------------------------
# run_bt_optional — 运行可选的 bpftrace 探针（失败不报错）
#
# 参数：同 run_bt
#
# 功能：
#   运行可选的探针脚本（如 kprobe），失败时不报错
#   结果标记为 OPTIONAL_RESULT=PASS 或 NOTE_FAILED_OR_UNSUPPORTED
#------------------------------
run_bt_optional() {
    local probe_file="$1" log_file="$2" duration="${3:-${BPFTRACE_DURATION}}"
    if run_bt "${probe_file}" "${log_file}" "${duration}"; then
        echo "OPTIONAL_RESULT=PASS" | tee -a "${log_file}"
        return 0
    fi
    echo "OPTIONAL_RESULT=NOTE_FAILED_OR_UNSUPPORTED" | tee -a "${log_file}"
    return 0
}