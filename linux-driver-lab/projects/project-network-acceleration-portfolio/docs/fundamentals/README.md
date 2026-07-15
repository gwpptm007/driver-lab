# 网络加速作品集：知识基础导航

这组文档不是新的 benchmark，也不替代各 track 的实验记录。它把 kernel netdev、XDP/AF_XDP、DPDK、eBPF、RDMA 和 SmartNIC/DPU 放在同一组工程问题中理解：

~~~
工作负载是什么？
数据在哪一段路径中移动？
谁拥有 buffer 与队列？
完成如何被确认？
哪个成本被绕开，又引入了什么新的约束？
什么证据足以支持当前结论？
~~~

## 阅读顺序

| 文档 | 回答的问题 |
| --- | --- |
| [00：15 分钟心智模型](00_15_MINUTE_MENTAL_MODEL.md) | 这些技术为什么不是互相替代的名词？ |
| [01：统一数据路径与成本](01_UNIFIED_DATAPATH_AND_COST_MODEL.md) | 一次包/字节搬运跨越了哪些边界？ |
| [02：技术选型框架](02_ACCELERATION_SELECTION_FRAMEWORK.md) | 何时选 kernel、XDP、AF_XDP、DPDK、RDMA 或 offload？ |
| [03：队列、内存与完成语义](03_QUEUES_MEMORY_AND_COMPLETION_SEMANTICS.md) | 高性能路径真正共享的正确性问题是什么？ |
| [04：控制面、数据面与观测面](04_CONTROL_DATA_OBSERVABILITY_PLANES.md) | 配置、转发和证据如何分层？ |
| [05：性能方法与 NUMA](05_PERFORMANCE_METHOD_AND_NUMA.md) | 怎样得到可比较、可复验的性能结论？ |
| [06：可观测性与故障定位](06_OBSERVABILITY_AND_DEBUGGING_PLAYBOOK.md) | 如何定位慢、丢、错与未 offload？ |
| [07：SmartNIC、DPU 与 representor](07_SMARTNIC_DPU_REPRESENTOR_AND_OFFLOAD.md) | host 路径如何演进到 eSwitch/offload？ |
| [08：可靠性、安全与多租户](08_RELIABILITY_SECURITY_AND_MULTITENANCY.md) | 高速路径如何保持隔离和可恢复？ |
| [09：证据等级与对外表述](09_EVIDENCE_LEVELS_AND_CLAIM_DISCIPLINE.md) | 什么能说验证，什么只能说设计/边界？ |
| [10：作品集项目地图与演示](10_PORTFOLIO_PROJECT_MAP_AND_DEMO.md) | 如何把各 track 串成项目级故事？ |
| [11：扩展路线与设计检查表](11_EXTENSION_ROADMAP_AND_DESIGN_CHECKLIST.md) | 如何把学习成果扩展为真实硬件项目？ |

## 使用边界

- 本目录只提供 Markdown 基础材料，不新增图像、GIF、性能数据或硬件结论。
- 具体通过记录以 [tests/EVIDENCE_INDEX.md](../../tests/EVIDENCE_INDEX.md) 和各 track 的原始记录为准。
- pcap PMD、veth、RXE/Soft-RoCE 证明的是相应的软件路径与语义；它们不自动升级为真实 NIC、RNIC 或 SmartNIC 性能/卸载结论。

如果只读一篇，读 [00：15 分钟心智模型](00_15_MINUTE_MENTAL_MODEL.md)；如果要做方案设计，从 [02：技术选型框架](02_ACCELERATION_SELECTION_FRAMEWORK.md) 和 [03：队列、内存与完成语义](03_QUEUES_MEMORY_AND_COMPLETION_SEMANTICS.md) 开始。
