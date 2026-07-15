# 11. 项目地图、复盘与扩展路线

本项目不是把 DPDK 与 verbs 放在同一个进程就结束，而是用一条受控的数据路径练习五个工程问题：边界、所有权、ABI、异步完成和可验证退出。

## 11.1 从阶段到代码的地图

| 阶段 | 核心问题 | 主要实现/证据 |
| --- | --- | --- |
| Phase 1 | 本地请求如何安全跨线程交接？ | `gateway_contract.*`、ring/slot/generation 单测 |
| Phase 2 | DPDK 包何时脱离 mbuf 生命周期？ | `gateway_ingress.*`、pcap 解析测试 |
| Phase 3 | RDMA WRITE 的本地/远端内存如何受控？ | `gateway_rdma_backend.*`、RXE RC/MR 验证 |
| Phase 4 | 两段路径如何闭合并安全停止？ | `gateway_rdma_worker.*`、端到端 64/48 计数 |
| Phase 5（规划） | 如何批量化且不丢完成归属？ | batch、CQ 策略、背压和 drain 验收 |
| Phase 6（规划） | 如何在真实设备上观察和恢复？ | eBPF/指标、硬件矩阵、错误场景 |

入口架构说明在 [`ARCHITECTURE.md`](../ARCHITECTURE.md)，实际实验命令和预期结果在 [`tests/TEST_FLOW.md`](../../tests/TEST_FLOW.md)。

## 11.2 读代码的推荐顺序

1. 先读 [00 速览](00_15_MINUTE_MENTAL_MODEL.md) 和 [01 边界](01_SYSTEM_BOUNDARIES_AND_DATA_PLANES.md)；
2. 再读 [02 内存所有权](02_MEMORY_OWNERSHIP_STAGING_AND_MR.md)、[03 ABI](03_DESCRIPTOR_WIRE_ABI_AND_VERSIONING.md)、[04 生命周期](04_SPSC_RING_GENERATION_AND_SLOT_LIFECYCLE.md)；
3. 对照 `gateway_contract.h/.c` 看每一个字段和状态转换；
4. 读 [05 ingress](05_DPDK_INGRESS_PARSER_AND_MBUF_BOUNDARY.md) 和 `gateway_ingress.c`；
5. 读 [06 RDMA](06_RDMA_RC_WRITE_MR_QP_AND_CQE.md)、[07 worker](07_WORKER_BACKPRESSURE_AND_GRACEFUL_DRAIN.md)，最后跟进 Phase 4 测试；
6. 在改造前回看 [08–10](08_ERROR_RECOVERY_AND_SECURITY_BOUNDARIES.md)，确保新功能有错误、性能和证据设计。

## 11.3 快速自检问题

| 问题 | 应能给出的答案 |
| --- | --- |
| 为什么 ring 里不放 `rte_mbuf *`？ | mbuf 在 ingress 末尾归还；异步 RDMA 需要独立、稳定且已注册/可管理的内存。 |
| 为什么 `slot_id` 不足以完成 slot？ | slot 可重用；旧 CQE 能迟到，必须加 generation 防止误释放。 |
| `ibv_post_send` 返回 0 就代表远端写成功吗？ | 不代表；它只说明本地接受 WR，异步结果由 CQE 表示。 |
| stop flag 后 worker 为什么不能立即退出？ | ring 与 SQ 可能仍有 READY/INFLIGHT 工作，需 drain 才能安全释放资源。 |
| RXE 通过说明了什么？ | 功能路径成立，不说明真实 RNIC 性能、拓扑或生产恢复能力。 |

## 11.4 扩展必须先改变契约

### 多 RX queue / 多 worker

先选择“一队列一 worker 一 QP”还是共享队列模型，随后重新定义 ring 所有者、slot 分区、flow affinity 和 CQE 到 slot 的唯一归属。不要直接把 SPSC 索引用于多个 lcore。

### 批量 WR 与 selective signaling

先引入 batch 元数据、outstanding 上限、signaled 边界与失败覆盖规则，再更改 poll 策略。验收标准必须包含关闭时所有 slot 的可解释结果。

### 真正零拷贝

先解决 mbuf external buffer 的注册、DMA 生命周期、mempool 引用、跨 NUMA 访问和 CQE 前回收，最后再评估 memcpy 是否真是瓶颈。它是内存所有权项目，不是一处 API 替换。

### 可靠交付/重试

先定义远端幂等、去重、持久确认与重连协商；然后才谈重发。否则任何“自动恢复”都可能重复写入。

## 11.5 完成一个改动前的检查

- 是否写清新字段、版本和字节序？
- 是否明确每块内存和每个资源的 owner？
- 是否给 READY/INFLIGHT/失败/停止路径提供回收规则？
- 是否能用计数或测试证明没有漏包、漏完成、漏 slot？
- 是否区分了 RXE 功能结论与真实硬件性能结论？

如果这些问题中有一项没有答案，优先补契约、文档和测试，而不是先扩大并发度。
