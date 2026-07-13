#!/usr/bin/env bash
set -euo pipefail

TRACK_ROOT=$(cd "$(dirname "$0")/.." && pwd)
PIPELINE_DIR="${TRACK_ROOT}/project-dpdk-flow-pipeline"
SKIP_CLEAN=${SKIP_CLEAN:-0}

cd "${TRACK_ROOT}"
bash tests/check_docs.sh

if [[ "${SKIP_CLEAN}" != "1" ]]; then
  make -C "${PIPELINE_DIR}" clean
fi

# flow pipeline 使用 pcap/null PMD，覆盖 contract、规则、双 worker、调优和错误边界。
make -C "${PIPELINE_DIR}" test-all

echo 'DPDK_ADVANCED_KNOWLEDGE_AND_SOFTWARE_REGRESSION_PASS'
