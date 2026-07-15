# EVIDENCE_INDEX

## 1. 总收口证据

```text
linux-driver-lab/project-linux-network-data-plane/docs/
linux-driver-lab/project-linux-network-data-plane/records/
```

重点：

- `docs/08_INTERVIEW_SHARE_SCRIPT.md`
- `docs/09_LIMITATIONS_AND_NEXT_STEPS.md`
- `docs/10_CROSS_PATH_COMPARISON.md`

## 2. DPDK

```text
linux-driver-lab/track-dpdk/ROADMAP_NEXT.md
linux-driver-lab/track-dpdk/docs/07_DPDK_TRACK_FINAL_STATUS.md
linux-driver-lab/track-dpdk/project-dpdk-media-gateway-lite/records/20260607-pcap-traffic-test/
linux-driver-lab/track-dpdk/project-dpdk-v17-legacy-review/docs/
```

关键状态：

```text
PASS_TRAFFIC
PASS_FORWARDING
PASS_REWRITE
PASS_REVIEW
```

## 3. DPDK Advanced

```text
linux-driver-lab/track-dpdk-advanced/project-dpdk-advanced-summary/reports/DPDK_ADVANCED_FINAL_REPORT.md
linux-driver-lab/track-dpdk-advanced/project-dpdk-advanced-summary/reports/EVIDENCE_INDEX.md
```

## 4. AF_XDP

```text
linux-driver-lab/track-af-xdp/README.md
linux-driver-lab/track-af-xdp/ROADMAP.md
linux-driver-lab/track-af-xdp/project-af-xdp-track-summary/
```

关键状态：

```text
all 4 phases PASS
```

## 5. eBPF Observability

```text
linux-driver-lab/track-ebpf-observability/README.md
linux-driver-lab/track-ebpf-observability/ROADMAP.md
```

关键状态：

```text
all phases COMPLETED
```

## 6. RDMA

```text
linux-driver-lab/track-rdma-core/README.md
linux-driver-lab/track-rdma-core/ROADMAP.md
linux-driver-lab/track-rdma-core/project-rdma-core-summary/EVIDENCE_INDEX.md
linux-driver-lab/track-rdma-core/project-rdma-rc-client-server/tests/TEST_RECORD_20260712_AFFINITY.md
linux-driver-lab/track-rdma-core/project-rdma-performance-tuning/tests/TEST_RECORD_20260713_NUMACTL_NODE0.md
```

关键状态：

```text
Phase 1~8 PASS
project-rdma-rc-client-server PASS current environment boundary
project-rdma-performance-tuning PASS_NUMACTL_NODE0_BINDING
```

## 7. 当前阻塞项

```text
134 SSH login blocked
135 only has node0
no real RNIC benchmark
no SmartNIC/DPU offload environment yet
```

## 8. 复验入口与结论级别

| 方向 | 当前结论 | 复验证据入口 | 升级结论所需条件 |
| --- | --- | --- | --- |
| DPDK fastpath | pcap PMD 逻辑闭环已验证 | `track-dpdk/project-dpdk-media-gateway-lite/records/20260607-pcap-traffic-test/` | 真实 NIC、固定包长、队列和 NUMA 的吞吐/时延记录 |
| AF_XDP | 4 phases PASS，zero-copy 受 veth 边界限制 | `track-af-xdp/project-af-xdp-track-summary/` | 支持 native/zero-copy 的真实 NIC 和驱动证据 |
| RDMA perf | RXE 参数和绑定框架已验证 | `track-rdma-core/project-rdma-performance-tuning/tests/TEST_RECORD_20260713_NUMACTL_NODE0.md` | 可登录双机、真实 RNIC、至少两个 NUMA node |
| SmartNIC/DPU | 仅完成实施地图 | `docs/06_SMARTNIC_DPU_MAP.md` | representor、`tc -s in_hw`、命中计数和 health 记录 |

统一复验流程见 `tests/REVALIDATION_CHECKLIST.md`。复验记录新增后，先更新此表，再更新简历或面试材料。

证据等级和十分钟演示路径见 `docs/07_PORTFOLIO_DEMO_RUNBOOK.md`。所有对外表述应使用与该条证据等级匹配的措辞。
