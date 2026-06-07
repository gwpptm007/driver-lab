#!/usr/bin/env bash
#===============================================================================
# verify_all_tracks.sh — 一键验证全部 7 个 track 的关键产物
#
# 用途: 对外展示前运行, 确认所有 track 的代码/二进制/报告/reports 存在
# 输出: 每行一个检查项, 状态标记 [OK]/[MISS]/[WARN]
# 退出码: 0=全部通过, 1=有缺失
#===============================================================================
set -euo pipefail

# --- 路径推导 ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
LAB_ROOT="$(cd "${PROJECT_ROOT}/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS=0
WARN=0
FAIL=0

check_file() {
  local desc="$1" path="$2"
  if [[ -f "${path}" ]]; then
    echo -e "  ${GREEN}[OK]${NC}    ${desc}"
    ((PASS++)) || true
  else
    echo -e "  ${RED}[MISS]${NC}  ${desc}  (expected: ${path})"
    ((FAIL++)) || true
  fi
}

check_dir() {
  local desc="$1" path="$2"
  if [[ -d "${path}" ]]; then
    echo -e "  ${GREEN}[OK]${NC}    ${desc}"
    ((PASS++)) || true
  else
    echo -e "  ${RED}[MISS]${NC}  ${desc}  (expected: ${path})"
    ((FAIL++)) || true
  fi
}

check_cmd() {
  local desc="$1" cmd="$2"
  if command -v "${cmd}" >/dev/null 2>&1; then
    echo -e "  ${GREEN}[OK]${NC}    ${desc}  ($(command -v "${cmd}"))"
    ((PASS++)) || true
  else
    echo -e "  ${YELLOW}[WARN]${NC}  ${desc}  (${cmd} not found)"
    ((WARN++)) || true
  fi
}

check_exec() {
  local desc="$1" path="$2"
  if [[ -x "${path}" ]]; then
    echo -e "  ${GREEN}[OK]${NC}    ${desc}"
    ((PASS++)) || true
  else
    echo -e "  ${YELLOW}[WARN]${NC}  ${desc}  (expected: ${path})"
    ((WARN++)) || true
  fi
}

echo "================================================================================"
echo " verify_all_tracks.sh — Linux Network Data Plane Portfolio"
echo " project root: ${PROJECT_ROOT}"
echo " lab root:     ${LAB_ROOT}"
echo "================================================================================"
echo ""

# ====== 1. Environment ======
echo "--- 1. Environment ---"
check_cmd "python3"        python3
check_cmd "bpftrace"       bpftrace
check_cmd "gcc"            gcc
check_cmd "make"           make
echo ""

# ====== 2. Foundation (W1-W5) ======
echo "--- 2. Foundation (day01-35) ---"
check_dir "foundation/     " "${LAB_ROOT}/foundation"
check_file "day01 demo.c   " "${LAB_ROOT}/foundation/day01/demo.c"
check_file "day22 PCIe     " "${LAB_ROOT}/foundation/day22/demo.c"
check_file "day29 DMA      " "${LAB_ROOT}/foundation/day29/demo.c"
check_file "foundation README" "${LAB_ROOT}/foundation/README.md"
echo ""

# ====== 3. Netdev (stage00-14) ======
echo "--- 3. Netdev (stage00-14) ---"
check_dir "netdev/        " "${LAB_ROOT}/netdev"
check_file "stage00 bootstrap" "${LAB_ROOT}/netdev/stage00_bootstrap/demo.c"
check_file "stage03 NAPI   " "${LAB_ROOT}/netdev/stage03_napi_poll/demo.c"
check_file "stage14 XDP    " "${LAB_ROOT}/netdev/stage14_xdp_basics/demo.c"
check_file "netdev README  " "${LAB_ROOT}/netdev/README.md"
echo ""

# ====== 4. Real Driver ======
echo "--- 4. Real Driver ---"
REAL_DRV="${LAB_ROOT}/track-real-driver"
check_dir "track-real-driver" "${REAL_DRV}"
check_file "virtio-net source dive" "${REAL_DRV}/lab-virtio-net-source-dive/README.md"
check_file "runtime observe  " "${REAL_DRV}/lab-virtio-net-runtime-observe/README.md"
check_file "ethtool patch    " "${REAL_DRV}/lab-virtio-net-ethtool-stats-mini-patch/README.md"
check_file "final patch report" "${REAL_DRV}/project-virtio-net-patch-and-trace/reports/final_project_report.md"
check_file "e1000e compare   " "${REAL_DRV}/lab-e1000e-source-compare/reports/e1000e_compare_report.md"
echo ""

# ====== 5. Virtual Net ======
echo "--- 5. Virtual Net ---"
VIRT="${LAB_ROOT}/track-virtual-net"
check_dir "track-virtual-net" "${VIRT}"
check_file "tap/bridge path  " "${VIRT}/lab-virtio-tap-bridge-path/reports/lab-virtio-tap-bridge-path_report.md"
check_file "vhost kick/notify" "${VIRT}/lab-virtio-vhost-kick-notify/reports/vhost_compare_report.md"
check_file "two-guest bridge " "${VIRT}/lab-two-guest-bridge-flow/reports/lab-two-guest-bridge-flow_report.md"
echo ""

# ====== 6. DPDK ======
echo "--- 6. DPDK ---"
DPDK="${LAB_ROOT}/track-dpdk"
check_dir "track-dpdk      " "${DPDK}"
check_exec "fastpath-lite   " "${DPDK}/project-user-space-fastpath/app/build/fastpath-lite"
check_exec "media-gateway-lite" "${DPDK}/project-dpdk-media-gateway-lite/app/build/media-gateway-lite"
check_dir "fastpath pcap records" "${DPDK}/project-fastpath-traffic-test/records/20260607_155955-fastpath-pcap"
check_dir "mgw pcap records " "${DPDK}/project-dpdk-media-gateway-lite/records/20260607-pcap-traffic-test"
check_file "DPDK track report" "${DPDK}/project-dpdk-track-summary/reports/final/DPDK_TRACK_REPORT.md"
echo ""

# ====== 7. AF_XDP ======
echo "--- 7. AF_XDP ---"
AFXDP="${LAB_ROOT}/track-af-xdp"
check_dir "track-af-xdp    " "${AFXDP}"
check_file "AF_XDP track summary" "${AFXDP}/project-af-xdp-track-summary/records/STATUS_SNAPSHOT_LATEST.md"
check_file "XDP redirect lab " "${AFXDP}/lab-xdp-redirect-basics/README.md"
check_file "socket rings lab " "${AFXDP}/lab-af-xdp-socket-rings/README.md"
check_file "mini forwarder  " "${AFXDP}/project-af-xdp-mini-forwarder/README.md"
echo ""

# ====== 8. eBPF Observability ======
echo "--- 8. eBPF Observability ---"
EBPF="${LAB_ROOT}/track-ebpf-observability"
check_dir "track-ebpf-obs  " "${EBPF}"
check_exec "net_observer    " "${EBPF}/project-linux-network-observability/build/net_observer"
check_file "BPF object      " "${EBPF}/project-linux-network-observability/build/net_observer.bpf.o"
check_file "observer source " "${EBPF}/project-linux-network-observability/src/net_observer.bpf.c"
check_file "observer report " "${EBPF}/project-linux-network-observability/reports/net-observe-20260606-193715.md"
echo ""

# ====== 9. This project ======
echo "--- 9. project-linux-network-data-plane ---"
check_file "README.md       " "${PROJECT_ROOT}/README.md"
check_file "final_report.md " "${PROJECT_ROOT}/reports/final_report.md"
check_file "resume_material " "${PROJECT_ROOT}/reports/resume_material.md"
check_file "architecture    " "${PROJECT_ROOT}/docs/07_FINAL_ARCHITECTURE.md"
check_file "interview script" "${PROJECT_ROOT}/docs/08_INTERVIEW_SHARE_SCRIPT.md"
check_dir "evidence/       " "${PROJECT_ROOT}/evidence"
echo ""

# ====== Summary ======
echo "================================================================================"
TOTAL=$((PASS + WARN + FAIL))
echo -e "Summary: ${GREEN}${PASS} OK${NC}  ${YELLOW}${WARN} WARN${NC}  ${RED}${FAIL} MISS${NC}  (${TOTAL} total checks)"
echo "================================================================================"

if [[ ${FAIL} -gt 0 ]]; then
  echo "Some checks FAILED. Review the [MISS] items above."
  exit 1
fi

echo "All critical checks passed."
exit 0
