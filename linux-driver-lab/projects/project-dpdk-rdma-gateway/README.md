# project-dpdk-rdma-gateway

## 知识基础文档

在阅读或重构实现前，建议先从 [`docs/fundamentals/README.md`](docs/fundamentals/README.md) 开始。该目录按数据面边界、内存所有权、SPSC/generation、DPDK ingress、RC WRITE/CQE、背压、错误恢复、性能与回归组织，明确区分当前 pcap + RXE 功能证据与未来真实 RNIC 项目能力。

DPDK-RDMA 综合 capstone：使用 DPDK 完成网络前端收包、解析和分类，通过有界 staging/ring 契约把请求交给 RDMA 后端，最终以 one-sided WRITE 写入远端服务内存。

当前阶段：`DPDK_RDMA_GATEWAY_CURRENT_ENV_COMPLETE`（Phase 1-4 全部 PASS）。

## 当前完成

- 定义固定 32 字节本地 request descriptor。
- 定义 40 字节显式大端 wire header，不直接发送 C 结构体。
- 定义 64 槽 SPSC request ring 和 release/acquire 内存序。
- 定义 cache-line 对齐的 2048 字节 staging slot。
- 定义 `FREE -> READY -> INFLIGHT -> FREE` 生命周期。
- 使用 `slot_id + generation` 拒绝 stale completion。
- 覆盖 wire roundtrip、6 类协议错误、ring 满/空/回绕和 slot 错误转换。
- 使用 pcap PMD 接收 Ethernet/IPv4/UDP，复制 UDP payload 后立即释放 mbuf。
- 64 包确定性流量中完成 48 个 UDP staging 请求，拒绝 16 个非 UDP 包。
- mock RDMA consumer 完成 48 次 wire encode/decode 与 slot generation 回收。
- 复用已验证的 `rdma_cs` 对象生命周期，在 RXE 上建立双进程 RC QP。
- client 将 40-byte header + 32-byte payload 真实 WRITE 到 server remote MR。
- client CQE 与 server remote record 验证均 PASS，并完成 slot generation 回收。
- 在同一 client 进程中以 DPDK 主线程生产 request、RDMA worker 消费并提交真实 WRITE。
- worker 在 producer stop 后排空 ring，48 个请求全部得到 CQE，所有 slot 回到 FREE。
- 135 上 clean build 后 Phase 1-4 联合回归通过。

## 快速测试

```bash
cd linux-driver-lab/projects/project-dpdk-rdma-gateway
make test
make test-phase2
make test-phase3
make test-phase4
make test-all
```

预期最终 marker：

```text
DPDK_RDMA_GATEWAY_PHASE1_CONTRACT_PASS
DPDK_RDMA_GATEWAY_PHASE2_INGRESS_PASS
DPDK_RDMA_GATEWAY_PHASE3_RDMA_PASS
DPDK_RDMA_GATEWAY_PHASE4_E2E_PASS
```

## 设计边界

Phase 4 已完成当前环境 capstone：pcap PMD、DPDK parser、bounded staging、SPSC ring、RDMA worker、RXE WRITE/CQE 与 remote MR 验证形成一条端到端路径。pcap PMD 与 Soft-RoCE 证明功能和并发契约，不代表真实 NIC/RNIC 的吞吐、DMA 或 NUMA 性能。

详细原理见 `docs/ARCHITECTURE.md`，路线图见 `ROADMAP.md`，完整命令见 `tests/TEST_FLOW.md`。

135 实测记录见 `tests/TEST_RECORD_20260713_PHASE1_CONTRACT.md`。

Phase 2 实测记录见 `tests/TEST_RECORD_20260713_PHASE2_INGRESS.md`。

Phase 3 实测记录见 `tests/TEST_RECORD_20260713_PHASE3_RDMA.md`。

Phase 4 与 Phase 1-4 clean regression 实测记录见 `tests/TEST_RECORD_20260713_PHASE4_E2E.md`。
