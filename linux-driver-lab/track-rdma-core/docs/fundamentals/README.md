# RDMA Fundamentals 学习入口

这套文档放在所有 RDMA 实验之前，用来解决一个常见问题：代码已经能跑，但脑中仍只有一串 `ibv_*` API。这里不按 API 字母表组织，而是按一次 RDMA 操作真正经过的层次组织。

## 学完后应能回答什么

1. RDMA 绕过了哪部分内核路径，又没有绕过哪些控制面？
2. `context/PD/MR/CQ/QP` 为什么必须按特定顺序创建和销毁？
3. `lkey/rkey` 分别保护谁，地址、权限和生命周期如何共同决定访问是否合法？
4. `WR/WQE/CQE` 分别属于软件描述、硬件队列和完成证据中的哪一层？
5. RC 的 QP 为什么必须经历 RESET、INIT、RTR、RTS？
6. SEND/RECV 与 RDMA READ/WRITE/Atomic 的远端 CPU 参与方式有何不同？
7. RoCEv2 为什么既需要 RDMA 对象知识，也需要 Ethernet/IP/UDP 和拥塞知识？
8. batch、inline、selective signaling、CQ polling、NUMA 分别在优化哪一种成本？
9. one-sided 系统为什么不能把“DMA 已完成”误认为“分布式事务已完成”？
10. 遇到 `IBV_WC_*` 错误时，怎样从环境、控制面、QP、MR、WR、网络逐层定位？

## 推荐阅读顺序

| 顺序 | 文档 | 建立的核心模型 |
| --- | --- | --- |
| 1 | [00_15_MINUTE_MENTAL_MODEL.md](00_15_MINUTE_MENTAL_MODEL.md) | 15 分钟形成完整轮廓 |
| 2 | [01_HARDWARE_KERNEL_USERSPACE_STACK.md](01_HARDWARE_KERNEL_USERSPACE_STACK.md) | RNIC、内核和用户态的边界 |
| 3 | [02_VERBS_OBJECTS_LIFECYCLE.md](02_VERBS_OBJECTS_LIFECYCLE.md) | verbs 对象图与逆序释放 |
| 4 | [03_MEMORY_REGISTRATION_DMA_KEYS.md](03_MEMORY_REGISTRATION_DMA_KEYS.md) | MR、DMA、IOMMU、lkey/rkey |
| 5 | [04_QP_STATE_MACHINE_CONTROL_PLANE.md](04_QP_STATE_MACHINE_CONTROL_PLANE.md) | QP 状态机与连接元数据 |
| 6 | [05_WR_WQE_CQE_DATA_PATH.md](05_WR_WQE_CQE_DATA_PATH.md) | WR 到 CQE 的端到端执行路径 |
| 7 | [06_TRANSPORTS_ROCE_NETWORK.md](06_TRANSPORTS_ROCE_NETWORK.md) | RC/UC/UD 与 RoCEv2 网络层 |
| 8 | [07_ONE_SIDED_ATOMIC_CONSISTENCY.md](07_ONE_SIDED_ATOMIC_CONSISTENCY.md) | one-sided、Atomic 与一致性协议 |
| 9 | [08_PERFORMANCE_TUNING_NUMA.md](08_PERFORMANCE_TUNING_NUMA.md) | 批处理、轮询、缓存和 NUMA |
| 10 | [09_RELIABILITY_SECURITY_FAILURES.md](09_RELIABILITY_SECURITY_FAILURES.md) | RNR、重试、超时、密钥和恢复 |
| 11 | [10_PROJECT_KNOWLEDGE_MAP.md](10_PROJECT_KNOWLEDGE_MAP.md) | 原理到本仓库代码/实验的映射 |
| 12 | [11_DEBUGGING_PLAYBOOK.md](11_DEBUGGING_PLAYBOOK.md) | 分层排障与证据采集 |
| 13 | [12_RECALL_CARDS.md](12_RECALL_CARDS.md) | 快速复习、面试自测 |

## 四条贯穿全程的主线

```mermaid
flowchart LR
    A[控制面<br/>发现设备与交换元数据] --> B[对象面<br/>PD MR CQ QP]
    B --> C[数据面<br/>post WR / doorbell / DMA]
    C --> D[完成面<br/>CQE / status / wr_id]
    D --> E[系统语义<br/>credit / version / recovery]
```

- **控制面**回答“对端是谁、资源参数是什么、QP 怎样进入可通信状态”。
- **对象面**回答“谁拥有资源、谁能访问哪块内存、完成送到哪里”。
- **数据面**回答“WQE 如何被 RNIC 消费，payload 如何通过 DMA 移动”。
- **系统语义**回答“完成之后数据是否可见、是否新鲜、失败后能否重试”。

## 学习边界

- Soft-RoCE 可以验证对象、状态机、协议语义和错误路径，不能代表真实 RNIC 的时延、PCIe/缓存行为和卸载能力。
- `ibv_post_send()` 返回成功只说明 WR 被软件接受，不说明网络传输和远端 DMA 已完成。
- CQE 成功说明该 WR 满足传输层完成语义，不自动提供业务事务、持久化或多对象原子性。
- PFC 不是 RoCE 的必要定义，也不是解决拥塞的万能开关；生产网络必须同时考虑 ECN/DCQCN、队列和故障域。

## 与旧文档的关系

原有 [../01_TRACK_OVERVIEW.md](../01_TRACK_OVERVIEW.md) 和 [../02_RDMA_CORE_MODEL.md](../02_RDMA_CORE_MODEL.md) 继续保留，作为历史总览与兼容入口；本目录是新的首选学习入口。项目执行从 [../../START_HERE.md](../../START_HERE.md) 继续。

