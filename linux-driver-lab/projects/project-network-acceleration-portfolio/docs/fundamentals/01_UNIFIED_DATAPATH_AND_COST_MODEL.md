# 01. 统一数据路径与成本模型

任何加速都可以拆成一条数据移动链：字节从哪块内存进入哪条队列，在哪里被 CPU 或 NIC 处理，何时被确认并回收。技术名称不同，成本主要落在同一类边界上。

## 1.1 三类数据路径

| 路径 | 典型链路 | 主要优势 | 主要成本 |
| --- | --- | --- | --- |
| kernel packet path | NIC -> DMA -> NAPI -> skb -> protocol stack -> socket | 通用、治理完整、可与内核生态组合 | skb、协议栈、软中断、socket copy/调度 |
| userspace packet path | NIC -> DMA -> PMD/UMEM -> userspace loop -> NIC | 避开通用协议栈、批量处理 | 专核轮询、buffer/queue 管理、设备/NUMA 约束 |
| RDMA data path | registered memory -> QP/WR -> RNIC -> remote MR -> CQE | 远端 DMA、低 CPU 数据搬运 | MR/QP/CQ、网络无损/拥塞、异步错误与访问控制 |

SmartNIC/DPU 并不单独构成第四种字节语义；它把上述某些 packet processing、switching、policy 或 control/service 逻辑移到 NIC eSwitch 或 DPU 核上。

## 1.2 成本不是只有 copy

常见优化往往只盯着 memcpy，但端到端成本至少包括：

~~~
DMA 与 PCIe
 + cache miss / cache line ownership
 + descriptor 填写与 doorbell
 + queue 同步与内存屏障
 + 分配/回收
 + 协议解析、分类、校验
 + 中断、软中断、调度
 + NUMA 远端访问
 + 完成轮询/事件处理
 + 控制面下发与统计
~~~

例如把 skb 替换为 mbuf 可避开部分协议栈成本，但若 worker 与 NIC 跨 NUMA，或 CQ 轮询造成 CPU 饥饿，系统仍可能更慢。优化必须先通过 profile、统计和对照定位成本所在。

## 1.3 同一报文在不同路径中的所有权

| 表示 | 典型 owner | 可否在异步完成前回收 | 常见错误 |
| --- | --- | --- | --- |
| sk_buff | kernel networking | 不可；引用与卸载状态必须完整 | 绕开协议栈后仍假设 skb 元数据存在 |
| DPDK mbuf | mempool / 应用 dataplane | 不可；TX/DMA 完成前需保持 | 跨线程/跨 burst 提前 free |
| AF_XDP UMEM frame | 用户态与内核 ring 契约 | 不可；completion ring 前不能重投 fill ring | fill/RX/TX/completion 角色混淆 |
| RDMA MR buffer | 应用与 RNIC | 不可；相关 WR 已完成/flush 前不能注销或复用 | 只凭 post_send 返回即复用 |
| eSwitch/offload state | NIC 硬件与控制面 | 不可假定下发即命中 | 只看 rule 创建，不看 in_hw 与 counter |

零拷贝只说明字节没有额外复制，不说明 buffer 可以无规则共享。大多数高性能故障都是所有权或完成边界错误。

## 1.4 完成有不同的含义

| 信号 | 实际表示 | 不表示什么 |
| --- | --- | --- |
| DPDK RX 返回 mbuf | 应用现在拥有收到的包 | 业务已经处理完成 |
| DPDK TX reclaim | NIC/driver 不再需要对应 buffer | 对端应用已消费 |
| AF_XDP completion | frame 可被用户态重新使用 | 对端协议已确认 |
| RDMA 成功 CQE | 本地 WR 的传输执行结果成功 | 远端业务持久化/事务提交 |
| tc counter 增长 | 某条规则匹配统计在增长 | 其他流量/整个策略全在硬件执行 |

工程设计要把可重用内存、本地传输成功、对端业务确认建模为不同状态，不能用一个 success 布尔值覆盖它们。

## 1.5 设计时应画出的最小表

每条数据路径至少应有下表，作为实现、测试和故障排查的共同语言：

| 阶段 | 数据表示 | owner | 队列/状态 | 成功条件 | 失败/回收 |
| --- | --- | --- | --- | --- | --- |
| 接收 | mbuf / UMEM frame / skb | RX consumer | RX queue | 长度与元数据有效 | 归还 pool/ring |
| 处理 | 解析后的 descriptor | worker | local ring | 规则/长度校验通过 | 丢弃并记原因 |
| 传输 | TX descriptor / WR | NIC/RNIC | TXQ/SQ | 本地接受投递 | 停止投递并清理 |
| 完成 | reclaim / CQE | completion owner | completion/CQ | 状态匹配 | 记录、隔离或重试 |

下一篇：[02：技术选型框架](02_ACCELERATION_SELECTION_FRAMEWORK.md)。
