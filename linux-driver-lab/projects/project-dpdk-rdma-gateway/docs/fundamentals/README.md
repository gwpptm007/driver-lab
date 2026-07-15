# DPDK-RDMA Gateway Fundamentals

这组文档是 `project-dpdk-rdma-gateway` 的项目级基础层。它建立“DPDK ingress 与 RDMA one-sided egress 如何通过明确内存契约组合”的模型，不重复 `track-dpdk` 的通用 EAL/mbuf 入门，也不重复 `track-rdma-core` 的通用 verbs 对象课。

当前项目已在 pcap PMD + RXE/Soft-RoCE 环境完成 Phase 1–4 功能闭环。这里的“完成”表示 contract、解析、SPSC handoff、RC WRITE/CQE、generation 回收和回归均有证据；不表示真实 NIC/RNIC 的吞吐、DMA、NUMA 或硬件 offload 已验证。

## 阅读顺序

| 顺序 | 文档 | 核心问题 | 对应实现 |
| --- | --- | --- | --- |
| 00 | [15 分钟心智模型](00_15_MINUTE_MENTAL_MODEL.md) | 为什么 mbuf 不能直接成为 RDMA payload？ | 全项目 |
| 01 | [系统边界与平面](01_SYSTEM_BOUNDARIES_AND_DATA_PLANES.md) | 哪些属于 DPDK、gateway contract、verbs 和远端服务？ | `docs/ARCHITECTURE.md` |
| 02 | [内存、所有权与注册](02_MEMORY_OWNERSHIP_STAGING_AND_MR.md) | mbuf、staging、send MR、remote MR 各由谁拥有？ | `gateway_ingress.c`、`gateway_rdma_backend.c` |
| 03 | [Descriptor 与 wire ABI](03_DESCRIPTOR_WIRE_ABI_AND_VERSIONING.md) | 本地 32 字节 descriptor 与远端 40 字节 header 为什么分开？ | `gateway_contract.c` |
| 04 | [SPSC ring、generation 与 slot 生命周期](04_SPSC_RING_GENERATION_AND_SLOT_LIFECYCLE.md) | 如何避免 stale CQE 释放被复用的 slot？ | `gateway_contract.c` |
| 05 | [DPDK ingress 与安全解析](05_DPDK_INGRESS_PARSER_AND_MBUF_BOUNDARY.md) | 何时 copy，何时释放 mbuf，如何统计拒绝？ | `gateway_ingress.c` |
| 06 | [RC WRITE、MR、QP 与 CQE](06_RDMA_RC_WRITE_MR_QP_AND_CQE.md) | WRITE 成功与远端应用可见性分别意味着什么？ | `gateway_rdma_backend.c` |
| 07 | [Worker、背压与优雅停止](07_WORKER_BACKPRESSURE_AND_GRACEFUL_DRAIN.md) | ring 满、slot 耗尽、CQ 预算和 shutdown 怎样处理？ | `gateway_rdma_worker.c` |
| 08 | [错误、恢复与安全](08_ERROR_RECOVERY_AND_SECURITY_BOUNDARIES.md) | 哪些错误可以重试，哪些必须隔离和重建？ | Phase 1–4 tests |
| 09 | [性能、NUMA 与测量方法](09_PERFORMANCE_NUMA_AND_MEASUREMENT.md) | 当前环境能测什么，不能测什么？ | `ROADMAP.md` |
| 10 | [观测与证据](10_OBSERVABILITY_EVIDENCE_AND_REGRESSION.md) | 怎样证明计数守恒、远端记录和回收正确？ | `tests/` |
| 11 | [项目地图与下一阶段](11_PROJECT_MAP_RECALL_AND_EXTENSION.md) | 如何从 Phase 4 扩到 batch、selective signaling 和真实硬件？ | `ROADMAP.md` |

## 不可破坏的设计契约

1. **跨运行时边界只传 descriptor，不传 `rte_mbuf *`。** mbuf 生命周期属于 PMD/mempool；verbs buffer 生命周期属于 MR/QP/CQ。当前用 copy 后的 staging slot 解耦。
2. **slot 只有 `FREE -> READY -> INFLIGHT -> FREE` 一条正常路径。** 每次复用递增 generation；completion 必须匹配 `slot_id + generation`。
3. **DPDK producer 不操作 verbs 对象；RDMA worker 独占 QP、CQ 和 send MR。** 这是当前 SPSC 和 thread ownership 前提，不可被“顺手加一个线程”破坏。
4. **本地 descriptor 是主机字节序；远端 wire header 显式大端编码。** 任何跨网络/跨版本字段都不能依赖 C struct layout。
5. **Phase 4 的成功是功能正确性证据。** pcap PMD 与 RXE 不能包装成真实 NIC/RNIC 的性能结果。

## 官方参考

- DPDK：[Ring Library](https://doc.dpdk.org/guides/prog_guide/ring_lib.html)、[Programmer's Guide](https://doc.dpdk.org/guides/prog_guide/index.html)
- RDMA verbs：[ibv_post_send(3)](https://man7.org/linux/man-pages/man3/ibv_post_send.3.html)、[RDMA programming guide](https://docs.nvidia.com/rdma-aware-networks-programming-user-manual-1-7.pdf)

完成本目录后，按 [tests/TEST_FLOW.md](../../tests/TEST_FLOW.md) 运行 Phase 1–4；功能和性能结论必须与对应 test record 一起阅读。
