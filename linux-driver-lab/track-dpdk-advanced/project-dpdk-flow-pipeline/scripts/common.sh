#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
APP_DIR="${PROJECT_ROOT}/app"
APP_BIN="${APP_DIR}/build/dpdk-flow-pipeline"
TOOLS_DIR="${PROJECT_ROOT}/tools"

# 所有运行脚本共享同一组可覆盖默认值，矩阵测试只修改目标变量。
: "${FLOW_LCORES:=0-2}"
: "${FLOW_MEMORY_CHANNELS:=4}"
: "${FLOW_FILE_PREFIX:=dpdk_flow_pipeline}"
: "${FLOW_PCAP_COUNT:=64}"
: "${FLOW_BURST_SIZE:=16}"
: "${FLOW_MBUF_CACHE:=250}"
: "${FLOW_EXTRA_RULES:=0}"
: "${FLOW_MAX_IDLE_POLLS:=100000}"
: "${FLOW_RECORD_DIR:=${PROJECT_ROOT}/tests/runtime}"

mkdir -p "${FLOW_RECORD_DIR}"
FLOW_PCAP_FILE="${FLOW_RECORD_DIR}/flow_input.pcap"
FLOW_LOG_FILE="${FLOW_RECORD_DIR}/flow_pipeline.log"
