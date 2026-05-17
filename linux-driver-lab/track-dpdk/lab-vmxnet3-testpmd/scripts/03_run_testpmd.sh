#!/usr/bin/env bash
# 脚本: 03_run_testpmd.sh
# 功能: 运行 dpdk-testpmd 进行 DPDK 网卡烟雾测试
# 用法: sudo ./scripts/03_run_testpmd.sh
# 环境变量:
#   TESTPMD_SECONDS    运行秒数（默认 20）
#   TESTPMD_CORES      使用的 CPU 核（默认 0-1）
#   TESTPMD_FORWARD_MODE 转发模式（默认 io）

set -euo pipefail
# 加载公共函数库
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 需要 root 权限（绑定网卡需要）
require_root_for_write
# 检查 DPDK_PCI 不能与管理 PCI 相同
guard_not_mgmt_pci

# 环境变量：运行时秒数、使用的 CPU 核数、内存通道数、转发模式、统计周期
: "${TESTPMD_SECONDS:=20}"
: "${TESTPMD_CORES:=0-1}"
: "${TESTPMD_MEM_CHANNELS:=4}"
: "${TESTPMD_FORWARD_MODE:=io}"
: "${TESTPMD_STATS_PERIOD:=5}"
: "${TESTPMD_EXTRA_EAL:=}"
: "${TESTPMD_EXTRA_ARGS:=}"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

# 查找 dpdk-testpmd 可执行文件
testpmd="$(find_testpmd || true)"
if [[ -z "${testpmd}" ]]; then
    echo "ERROR: testpmd not found. Install dpdk-testpmd or set TESTPMD_BIN=/path/to/dpdk-testpmd" >&2
    exit 1
fi

OUT="${RECORD_DIR}/TESTPMD.log"
: > "${OUT}"

# 记录测试参数
{
    echo "# TESTPMD"
    echo "date=$(date '+%F %T')"
    echo "testpmd=${testpmd}"
    echo "DPDK_PCI=${DPDK_PCI}"
    echo "TESTPMD_SECONDS=${TESTPMD_SECONDS}"
    echo "TESTPMD_CORES=${TESTPMD_CORES}"
    echo "TESTPMD_MEM_CHANNELS=${TESTPMD_MEM_CHANNELS}"
    echo "TESTPMD_FORWARD_MODE=${TESTPMD_FORWARD_MODE}"
    echo
} >> "${OUT}"

# 构建 testpmd EAL 参数：指定 CPU 核(-l)、内存通道数(-n)、网卡 PCI 地址(-a)
# -l 0-1          使用 CPU 0 和 1 核运行 testpmd
# -n 4            内存通道数为 4（影响 DPDK 内存分配策略）
# -a 0000:0b:00.0 将指定 PCI 设备添加到 DPDK（此时 ens192 已绑定到 vfio-pci）
CMD=(
    "${testpmd}"
    -l "${TESTPMD_CORES}"
    -n "${TESTPMD_MEM_CHANNELS}"
    -a "${DPDK_PCI}"
)

# shellcheck disable=SC2206
# 可选：追加额外的 EAL 参数（如 --no-huge）
EXTRA_EAL=( ${TESTPMD_EXTRA_EAL} )
if [[ "${#EXTRA_EAL[@]}" -gt 0 ]]; then
    CMD+=( "${EXTRA_EAL[@]}" )
fi

# 构建 testpmd 转发参数（-- 之后的参数为 testpmd 自身参数）
# --port-topology=chained  端口拓扑为链式（每个 port 独立队列）
# --forward-mode=io        转发模式为纯 I/O（收包直接转发，不做复杂处理）
# --auto-start             启动后自动开始转发（无需手动执行 start 命令）
# --stats-period=5         每 5 秒打印一次端口统计信息（收发包数、丢包等）
CMD+=(
    --
    --port-topology=chained
    --forward-mode="${TESTPMD_FORWARD_MODE}"
    --auto-start
    --stats-period="${TESTPMD_STATS_PERIOD}"
)

# shellcheck disable=SC2206
# 可选：追加额外的 testpmd 参数
EXTRA_ARGS=( ${TESTPMD_EXTRA_ARGS} )
if [[ "${#EXTRA_ARGS[@]}" -gt 0 ]]; then
    CMD+=( "${EXTRA_ARGS[@]}" )
fi

{
    echo "## Command"
    # printf '%q ' 将命令和参数按 quoted 格式输出，便于复现
    printf '%q ' timeout "${TESTPMD_SECONDS}" "${CMD[@]}"
    echo
    echo
    echo "## Output"
} >> "${OUT}"

# 运行 testpmd，限时 TESTPMD_SECONDS 秒
# set +e 临时关闭 errexit，以便捕获 testpmd 的退出码
set +e
timeout "${TESTPMD_SECONDS}" "${CMD[@]}" >> "${OUT}" 2>&1
rc=$?
set -e

# 记录退出码（124 = timeout 正常终止，非错误）
{
    echo
    echo "## Exit"
    echo "timeout/testpmd rc=${rc}"
    if [[ "${rc}" -eq 124 ]]; then
        echo "NOTE: rc=124 means timeout stopped testpmd after TESTPMD_SECONDS; this is acceptable for timed smoke test."
    fi
} >> "${OUT}"

# 非零且非 timeout 退出码视为失败
if [[ "${rc}" -ne 0 && "${rc}" -ne 124 ]]; then
    echo "ERROR: testpmd failed rc=${rc}. See ${OUT}" >&2
    exit "${rc}"
fi

cat <<EOF

[OK] testpmd smoke log saved:
${OUT}

Next:
  ./scripts/04_collect_stats.sh
EOF
