# 00. 15 分钟心智模型：网络加速不是一条单线

网络加速通常不是把包变得神奇地更快，而是有意识地改变以下三件事：

1. **路径**：是否经过内核协议栈、socket、软中断和通用调度；
2. **所有权**：哪一方拥有 packet buffer、DMA buffer、队列和完成状态；
3. **执行位置**：host CPU、NIC 硬件、DPU 或远端 RNIC 分别做什么。

同一应用可能同时使用其中多项：控制面仍是 Linux netdev 和 netlink；报文分类在 XDP；大流量转发在 DPDK；远端存储/消息使用 RDMA；eBPF 提供旁路观测；eSwitch 把部分转发或 ACL 下沉到 SmartNIC。

## 0.1 一张文字地图

~~~
Linux netdev/NAPI
  适合：通用网络、驱动与控制面、成熟生态
  成本：协议栈、skb 分配/释放、软中断与上下文切换

XDP / AF_XDP
  适合：尽早丢弃、重定向、用户态 socket 与内核协作
  成本：UMEM/ring 生命周期、驱动能力、zero-copy 条件

DPDK
  适合：专用 CPU 核上的用户态 packet processing / forwarding
  成本：PMD 轮询、设备绑定、hugepage、CPU 与 NUMA 专用化

RDMA
  适合：低 CPU 的远端内存访问和消息传递
  成本：MR、QP/CQ、连接状态、RNIC/RoCE 网络与完成语义

SmartNIC / DPU
  适合：把隔离、转发、策略或基础服务放到 host 外
  成本：representor/eSwitch、硬件资源、offload 覆盖与可运维性

eBPF
  适合：观测、策略和受约束的内核可编程性
  成本：验证器限制、事件开销、版本/attach 点差异
~~~

它们解决的对象不同，因此 DPDK 比 XDP 快 或 DPU 替代 RDMA 都不是完整问题。首先要问：包处理、远端数据搬运、租户隔离、可观测性，究竟是哪一个瓶颈或目标？

## 0.2 四条不能省略的主线

### 路径主线

从线速包到业务逻辑，可能经过 NIC queue、DMA、NAPI、XDP、skb、socket，也可能进入 PMD/mbuf，或由 eSwitch 直接转发。任何加速设计都要画出实际路径，并标出哪一段被绕过。

### 队列主线

RX/TX queue、AF_XDP fill/completion ring、DPDK ring、RDMA SQ/RQ/CQ 都是生产者与消费者之间的契约。队列深度、并发模型、满/空行为和内存序决定正确性与尾延迟。

### 内存主线

skb、mbuf、UMEM frame、registered MR 看似不同，核心问题相同：谁分配、谁填充、何时 DMA、谁在何时回收。没有明确所有权的零拷贝只会把 bug 变成偶发内存破坏。

### 证据主线

一个命令成功、一个程序退出或一条规则存在，都不足以证明性能或卸载。必须记录输入、环境、观测点、计数守恒和结论等级。

## 0.3 用一个判断循环做设计

每次考虑新技术，都依次回答：

| 问题 | 示例 |
| --- | --- |
| 工作负载 | 小包 L4 转发、DDoS 过滤、RPC、存储复制还是 VM 东西向流量？ |
| 目标 | 降 CPU、降 p99、提高 pps、隔离租户，还是提高可观测性？ |
| 当前成本 | skb 路径、copy、锁、跨 NUMA、CQE、控制面下发还是远端等待？ |
| 候选机制 | XDP、AF_XDP、DPDK、RDMA、tc offload、DPU service？ |
| 新约束 | 专核、内存注册、硬件依赖、队列所有权、升级/回滚？ |
| 证据 | 什么计数、trace、硬件状态和对照实验可以证明它生效？ |

先完成这一循环，才讨论 API、burst size 或硬件型号。

## 0.4 本作品集当前能证明什么

当前仓库已经沉淀 kernel netdev、real driver、virtual net、DPDK、AF_XDP、eBPF 与 RDMA 的路径学习和相应实验材料；组合型 DPDK-RDMA Gateway 也已在 pcap + RXE 边界下完成端到端功能验证。

当前不能证明真实 NIC 线速、RNIC 跨主机性能、AF_XDP zero-copy、tc flower 硬件命中或 SmartNIC/DPU offload 已完成。把这条界线讲清楚，是作品集可信度的一部分。

下一篇：[01：统一数据路径与成本](01_UNIFIED_DATAPATH_AND_COST_MODEL.md)。
