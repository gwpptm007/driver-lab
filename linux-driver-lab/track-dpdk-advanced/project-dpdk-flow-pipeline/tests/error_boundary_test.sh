#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
source "${PROJECT_ROOT}/scripts/common.sh"
cd "${PROJECT_ROOT}"

BOUNDARY_DIR="${FLOW_RECORD_DIR}/boundary"
mkdir -p "${BOUNDARY_DIR}"
bash scripts/01_build.sh
python3 "${TOOLS_DIR}/gen_flow_pcap.py" "${FLOW_PCAP_FILE}" 64

run_failure_case() {
    local name=$1
    local marker=$2
    local log="${BOUNDARY_DIR}/${name}.log"
    local status
    shift 2

    # 失败路径本身是测试目标，因此临时关闭 errexit 并显式校验退出码。
    set +e
    "${APP_BIN}" "$@" >"${log}" 2>&1
    status=$?
    set -e

    if [[ ${status} -eq 0 ]]; then
        echo "FAIL: boundary case ${name} unexpectedly succeeded" >&2
        return 1
    fi
    grep -q "${marker}" "${log}"
    grep -q 'cleanup=complete result=fail' "${log}"
    echo "FLOW_BOUNDARY_CASE_PASS name=${name} status=${status} marker=${marker}"
}

# 参数语法合法但违反四类流量等分模型。
run_failure_case expected_packets \
    'FLOW_CONFIG_BOUNDARY_REJECT reason=expected_packets value=63' \
    -l 0-2 -n "${FLOW_MEMORY_CHANNELS}" --no-pci --no-huge \
    --file-prefix "${FLOW_FILE_PREFIX}_boundary_expected_$$" \
    --vdev "net_pcap0,rx_pcap=${FLOW_PCAP_FILE}" --vdev net_null1 -- \
    --expected-packets 63

# 三条业务规则之外最多允许 FLOW_TABLE_MAX_RULES - 3 条填充规则。
run_failure_case extra_rules \
    'FLOW_CONFIG_BOUNDARY_REJECT reason=arguments' \
    -l 0-2 -n "${FLOW_MEMORY_CHANNELS}" --no-pci --no-huge \
    --file-prefix "${FLOW_FILE_PREFIX}_boundary_rules_$$" \
    --vdev "net_pcap0,rx_pcap=${FLOW_PCAP_FILE}" --vdev net_null1 -- \
    --expected-packets 64 --extra-rules 1022

# 只创建输入端口，验证转发端口缺失时在资源初始化前拒绝运行。
run_failure_case insufficient_ports \
    'FLOW_PORT_BOUNDARY_REJECT available=1 required=2' \
    -l 0-2 -n "${FLOW_MEMORY_CHANNELS}" --no-pci --no-huge \
    --file-prefix "${FLOW_FILE_PREFIX}_boundary_ports_$$" \
    --vdev "net_pcap0,rx_pcap=${FLOW_PCAP_FILE}" -- \
    --expected-packets 64

# main lcore 之外只剩一个 worker，双 worker 模型必须明确失败。
run_failure_case insufficient_workers \
    'FLOW_WORKER_BLOCKED available=1 required=2' \
    -l 0-1 -n "${FLOW_MEMORY_CHANNELS}" --no-pci --no-huge \
    --file-prefix "${FLOW_FILE_PREFIX}_boundary_workers_$$" \
    --vdev "net_pcap0,rx_pcap=${FLOW_PCAP_FILE}" --vdev net_null1 -- \
    --expected-packets 64

echo 'DPDK_FLOW_PIPELINE_PHASE6_BOUNDARY_PASS'
echo 'PASS: DPDK flow pipeline failure boundaries and cleanup'
echo 'script_summary name=error_boundary_test status=pass'
