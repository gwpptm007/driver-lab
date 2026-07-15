# ROADMAP

## 总目标

构建一个可解释、可观测、可回归的 DPDK ingress + RDMA egress 网关，证明用户态 packet fast path 与 verbs one-sided data path 如何通过明确的内存和所有权契约组合，而不是把两个独立 demo 简单拼接。

## 分阶段计划

| Phase | 目标 | 验收状态 |
|---|---|---|
| 1 | request/wire/staging/ring/slot generation contract | PASS |
| 2 | pcap PMD ingress，解析 UDP 并写入 staging | PASS |
| 3 | RXE RDMA backend，独立完成 staging 到 remote MR 的 WRITE | PASS |
| 4 | DPDK producer + RDMA consumer 端到端集成与 CQ 回收 | PASS |
| 5 | batch WR、selective signaling、CQ polling 与 backpressure 调优 | FUTURE_EXTENSION |
| 6 | eBPF/软件统计观测、错误注入与真实硬件复验 | FUTURE_EXTENSION |

## 当前环境收口结论

1. Phase 1-4 在 135 上使用 `-Wall -Wextra -Werror` clean build 并回归通过。
2. 64 个输入包产生 48 个 UDP request 和 48 个成功 CQE，计数守恒。
3. DPDK 主线程不操作 verbs 对象；RDMA worker 独占 QP/CQ。
4. worker drain 后 `active_slots=0`，remote MR 最终记录验证通过。

项目基线因此标记为 `DPDK_RDMA_GATEWAY_CURRENT_ENV_COMPLETE`。Phase 5/6 是性能与硬件证据增强，不阻塞当前功能 capstone 收口。

## 最终验收指标

- 功能：packet count、request count、RDMA completion、remote record 四者守恒。
- 正确性：无 slot 重用竞态、无 stale CQE 误释放、无 mbuf/MR 生命周期泄漏。
- 性能：吞吐、端到端 p50/p99、ring occupancy、CQ batch 和 drop/backpressure。
- 边界：Soft-RoCE 数据不包装成 RNIC 性能，pcap PMD 不包装成真实 NIC RSS。
