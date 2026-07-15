# 05. 性能方法与 NUMA：先让数字可信

性能数字不是产品能力本身，而是在给定硬件、流量、亲和性、队列和软件版本下的一次观察。没有这些上下文，pps、Gbps 和 p99 都不能比较。

## 5.1 每次结果的最小实验合同

| 类别 | 必须记录 |
| --- | --- |
| 软件 | git revision、build flags、kernel、DPDK、rdma-core、驱动/firmware 版本 |
| 硬件 | NIC/RNIC 型号、PCI BDF、NUMA node、CPU、内存、链路速率/MTU |
| 流量 | packet/WR 大小、协议混合、flow 数、方向、持续时间、warm-up |
| 运行 | lcore/worker affinity、IRQ、governor、hugepage/UMEM/MR 配置 |
| 策略 | burst/batch、polling、inline、selective signaling、RSS/queue 数 |
| 结果 | 吞吐、p50/p99/p999、CPU、drop/error、queue 高水位及原始样本 |

这些信息应与结果一起存档，而非在几周后通过记忆补写。

## 5.2 先选正确的指标

| 目标 | 主指标 | 必须搭配的护栏 |
| --- | --- | --- |
| 高吞吐转发 | pps / Gbps | packet size、drop、CPU、队列占用、p99 |
| 低延迟 RPC | p50/p99/p999 | 并发度、tail 样本量、CPU、服务端状态 |
| RDMA 传输 | ops/s、延迟、bytes | CQE error、outstanding WR、MR/QP 参数 |
| offload | host CPU 与规则命中 | in_hw、硬件/端口统计、fallback/error |
| 稳定性 | 长时间错误率、资源增长 | buffer/slot 泄漏、health、重连/恢复次数 |

平均值不能掩盖 p99，吞吐不能掩盖丢包，CPU 降低也不能掩盖把工作转移到未计入的 DPU/NIC。

## 5.3 NUMA 是数据路径的一部分

理想情况下，NIC/RNIC PCIe 所在 socket、RX/TX lcore、DPDK mempool 或 AF_XDP UMEM、RDMA MR、CQ poller 和应用 worker 位于同一 NUMA node。跨 node 的内存与 PCIe 访问会增加带宽竞争和尾延迟。

应分别测量：

1. 同 node 的 CPU、内存和设备绑定；
2. CPU 跨 node、内存仍本地；
3. 内存跨 node、CPU 仍本地；
4. 必要时设备/IRQ/queue 重新绑定后的差异。

只有一台单 node 或只有 node0 可用时，可以验证绑定方法，不应声称已经完成跨 NUMA 对比。

## 5.4 控制变量的方法

一次实验只改一个主要变量：例如 burst size、queue 数、inline 阈值或 CPU affinity。每种配置先 warm-up，再多次独立运行，报告中位数与离散范围；遇到离群结果时保留原始数据并解释，不要只选最好的一次。

baseline 必须仍可运行。若优化后提升 20%，但没有同一二进制、同一流量、同一 CPU/NUMA 条件下的 baseline，这个结论没有解释力。

## 5.5 环境边界

| 环境 | 可以得出的结论 | 不可以得出的结论 |
| --- | --- | --- |
| pcap PMD | DPDK pipeline 的解析/分类/转发逻辑 | 真实 NIC 线速和 DMA 性能 |
| veth | XDP/AF_XDP 协议与 ring 语义 | 驱动 native/zero-copy 能力 |
| RXE/Soft-RoCE | verbs、MR/QP/CQ、完成和错误模型 | RNIC DMA、RoCE 拥塞、跨机性能 |
| 真实 NIC/RNIC | 已记录条件下的设备性能 | 其他型号、其他 topology 的泛化结论 |

这不是贬低软件环境，而是让每一层实验承担它适合证明的内容。

## 5.6 性能检查清单

- 流量发生器是否成为瓶颈？
- 收发两端是否同时记录了 drop/error？
- CPU 是否被固定到预期 core，频率是否稳定？
- RSS/queue 与 worker 是否有明确的 flow affinity？
- 内存、NIC/RNIC、worker 是否同 NUMA node？
- 被比较的两组是否拥有相同包长、并发、时长和 warm-up？
- 是否明确未测量的成本，如控制面、DPU CPU、加密或业务处理？

下一篇：[06：可观测性与故障定位](06_OBSERVABILITY_AND_DEBUGGING_PLAYBOOK.md)。
