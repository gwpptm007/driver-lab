# 11. 扩展路线与设计检查表

从学习仓库进入项目级数据面，扩展顺序应先补证据与契约，再增加并发和硬件能力。下面路线可作为后续迭代的准入条件。

## 11.1 推荐路线

| 阶段 | 目标 | 先决条件 | 退出条件 |
| --- | --- | --- | --- |
| P0 契约收口 | 统一 buffer、queue、完成与错误文档 | 单路径测试可运行 | 所有资源有 owner 与回收路径 |
| P1 可观测 | 指标、错误样本、环境快照 | 可识别输入与输出 | 计数可守恒，问题可定位到阶段 |
| P2 真实 host NIC | DPDK/AF_XDP/RSS/NUMA 对照 | 独立 NIC、可恢复绑定 | 同条件 baseline 与硬件统计齐全 |
| P3 真实 RNIC | RC/UD、MR、CQ、拥塞/NUMA 对照 | 双机、RNIC、网络配置 | 不再引用 RXE 替代 RNIC 结论 |
| P4 SmartNIC/DPU | representor、tc offload、健康/资源 | switchdev 支持、回滚方案 | in_hw、counter、health 和 fallback 证据闭合 |
| P5 多租户与长稳 | 配额、隔离、故障恢复 | 前述资源模型与指标 | 压力、升级、故障演练有记录 |

每一阶段应保留前一阶段的测试。性能优化不能删除生命周期或错误路径断言。

## 11.2 每项设计的准入检查

### 路径和语义

- 输入/输出协议、顺序、丢包和重试语义是否明确？
- 哪些包走 kernel、XDP、userspace、RDMA 或 eSwitch？
- local completion 与 remote/business completion 是否分开？

### 队列和内存

- 每个 queue 的并发模型、容量、满/空策略和内存序是否明确？
- 每类 buffer 的 owner、DMA 生命周期、generation 和回收点是否明确？
- 多 worker/multi-queue 是否有 flow affinity 与唯一 completion owner？

### 控制和运维

- 配置是否版本化，是否可原子切换和回滚？
- 硬件资源耗尽、规则拒绝、QP error、设备 health 告警如何处理？
- 有没有不会阻塞 dataplane 的指标、日志和健康接口？

### 性能和证据

- 是否存在同条件 baseline？
- NUMA、CPU affinity、queue、硬件版本和流量是否被记录？
- 结论是否被正确标记为软件路径、单机硬件或部署级证据？

## 11.3 何时停止优化

当当前瓶颈不在数据路径、性能收益低于运维成本、无法提供隔离/回滚、或新机制不能得到可靠证据时，应停止下沉而改进基线、可观测性或架构。网络加速的成熟表现不是总能选到最复杂的技术，而是能证明为什么此处不该再加速。

## 11.4 与原作品集文档的关系

本目录提供的是项目级基础与检查表；具体成果地图、DPDK/RDMA 对比、调优边界、面试材料和 SmartNIC/DPU 操作计划仍位于上一级 docs。测试结论以 tests 中的证据索引和复验清单为准。

回到导航：[知识基础 README](README.md)。
