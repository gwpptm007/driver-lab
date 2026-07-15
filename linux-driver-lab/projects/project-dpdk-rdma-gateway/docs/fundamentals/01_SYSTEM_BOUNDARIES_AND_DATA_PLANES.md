# 01：系统边界与数据平面

## 四个平面，四类责任

| 平面 | 当前实现 | 责任 | 不负责什么 |
| --- | --- | --- |
| DPDK ingress | `gateway_dpdk_ingress`、`gateway_ingress.c` | RX、L2/L3/L4 验证、UDP payload copy、统计 | QP/CQ、remote rkey、RDMA completion |
| gateway contract | `gateway_contract.c` | descriptor、wire codec、ring、slot generation | 解析 mbuf、执行 verbs |
| RDMA egress | `gateway_rdma_backend.c`、worker | QP 建链、MR、WRITE、CQ poll、slot completion | PMD RX、mbuf free |
| remote service | `gateway_rdma_server` | remote MR、wire decode、payload verification | 接收本地 descriptor、管理 ingress ring |

当前 Phase 4 让 DPDK 主线程与 RDMA pthread 在同一进程，但同进程不意味着共享所有权。模块边界比地址空间边界更重要。

## control plane 与 data plane

### Control plane

- 选择 RDMA device/RXE、创建 context/PD/MR/CQ/QP；
- TCP 交换 QPN、PSN、GID、remote address/rkey；
- QP 状态从 RESET 到 INIT、RTR、RTS；
- 初始化 ring、slot pool、pcap PMD 和 worker；
- stop、drain、join、destroy。

### Data plane

- `rte_eth_rx_burst()` 取得 mbuf；
- parser 识别完整 UDP；
- copy payload 到 staging，发布 descriptor；
- worker 编码 header、post signaled WRITE、poll CQ；
- generation 匹配后回收 slot。

control plane 成功只表示对象已就绪；data plane 成功必须由计数守恒、CQE 和 remote record 共同证明。

## 为什么边界不是 `rte_mbuf *`

直接将 mbuf 地址放入 `ibv_sge` 会同时引入：多段 mbuf、PMD/mempool 生命周期、DMA/IOMMU 注册、跨线程释放、失败回收和 NIC/RNIC 内存可达性。它可能成为未来零拷贝优化方向，但不能作为当前功能 capstone 的默认接口。

当前边界是：

```text
mbuf (ingress-private)
  -- bounded copy --> staging slot (gateway-owned)
  -- descriptor --> RDMA worker
  -- wire encode --> registered send buffer
  -- WRITE --> remote MR
```

每一次跨边界都能明确“谁拥有 payload、何时可以释放、失败后谁回收”。

## 可扩展的替换点

| 未来变化 | 替换的平面 | 应保持不变的契约 |
| --- | --- | --- |
| 真实 NIC PMD | ingress source | parser 结果、slot/ring 统计与所有权 |
| DPDK `rte_ring` | local ring implementation | 单/多 producer 模型、full/empty 语义 |
| batch WR | RDMA worker posting strategy | slot 在相关 completion 前不能释放 |
| vhost/AF_XDP ingress | producer implementation | staging descriptor 契约 |
| RNIC | transport device | remote addr/rkey、CQE 与能力边界 |
| persistent remote log | remote service | wire ABI、ack/persistence protocol 明确化 |

扩展的原则是替换一层，而不是跨层传递内部对象。

## 当前可证明的端到端结果

Phase 4 的 48 条 UDP request 由一个 producer、一个 worker 依序完成 RC WRITE；所有 CQE 成功，worker drain 后 active slots 为零，server 验证最后 remote record。这证明函数组合正确和局部生命周期可闭合。

它尚未证明多 worker、多个 QP、乱序 completion、远端多 record、故障恢复或真实硬件性能；这些属于后续的显式扩展，不应被文档偷换为“已经支持”。
