#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "${PROJECT_ROOT}"

# Phase 1 无 DPDK/RDMA 运行时依赖，先独立验证跨线程和 wire 契约。
make clean
make
./build/gateway-contract-test | tee tests/contract_test.log

grep -q 'GATEWAY_CONTRACT_LAYOUT_PASS' tests/contract_test.log
grep -q 'GATEWAY_WIRE_ROUNDTRIP_PASS' tests/contract_test.log
grep -q 'GATEWAY_WIRE_BOUNDARY_PASS cases=6' tests/contract_test.log
grep -q 'GATEWAY_RING_SPSC_PASS requests=256 full_empty_wrap=pass' tests/contract_test.log
grep -q 'GATEWAY_SLOT_LIFECYCLE_PASS stale_completion=blocked generation=2' tests/contract_test.log
grep -q 'DPDK_RDMA_GATEWAY_PHASE1_CONTRACT_PASS' tests/contract_test.log

echo 'PASS: DPDK-RDMA gateway Phase 1 contract and lifecycle'
echo 'script_summary name=contract_test status=pass'
