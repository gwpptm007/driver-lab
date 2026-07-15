# 02. 技术选型框架：不是从工具名开始

选择网络加速技术时，先定义问题，再选最少的机制。为一个普通 socket 服务引入 DPDK 专核，或为低频可观测性问题引入 RDMA，都会把系统复杂度转移到运维与可靠性上。

## 2.1 先按工作负载分流

| 工作负载/目标 | 首选思路 | 何时升级 |
| --- | --- | --- |
| 通用 TCP/UDP 服务、完整协议与治理 | kernel netdev + socket | profile 证明协议栈或 copy 为主瓶颈 |
| 尽早丢弃/采样/重定向 | XDP | 需要用户态批处理或自管 buffer |
| 用户态包处理但希望保留 XDP/内核协作 | AF_XDP | 需要设备独占、极致 packet loop 或成熟 PMD 生态 |
| 专用转发、L2-L4 pipeline、固定专核 | DPDK | 远端内存操作而非 packet loop 成为核心 |
| RPC/存储/复制中的远端数据搬运 | RDMA | 需要把策略/转发/隔离下沉到 NIC/DPU |
| 大量 VM/CNFs 的东西向策略和转发 | tc/eSwitch/SmartNIC/DPU | host CPU、延迟或隔离需求有量化证据 |
| 路径诊断与长期可观测性 | eBPF + 指标/trace | 高速全量数据需要专门采样或硬件 telemetry |

表中升级意味着新的事实已出现，不是技术更高级。

## 2.2 六个选择维度

### 协议与业务语义

DPDK 不提供 TCP stack；RDMA WRITE 也不等于应用确认。若业务依赖连接管理、拥塞语义、TLS、复杂重传或 socket 生态，kernel 路径可能仍是合理选择。

### CPU 模型

DPDK 常以 polling 换低延迟和可预测性，代价是 core 长期忙等。AF_XDP 可以在 busy-poll 与事件之间取舍。RDMA 可减少数据搬运 CPU，但 CQ polling、控制面和内存注册仍需要 CPU。必须先明确 CPU 是否可专用。

### 内存与 DMA

需要判断缓冲区是否长期稳定、是否可 hugepage/UMEM/MR 注册、是否跨 NUMA、是否允许 pin memory。无法满足内存生命周期约束时，所谓零拷贝不应成为目标。

### 硬件与部署权限

DPDK 需要合适 PMD/VFIO/IOMMU 条件；AF_XDP zero-copy 依赖驱动；RDMA 依赖 RNIC/provider 与网络配置；SmartNIC offload 需要 switchdev/representor/firmware。部署团队是否能提供、升级和回滚这些能力是选型输入。

### 可运维性

专核、设备独占、eSwitch 模式切换、PFC/ECN、固件升级都会改变故障域。若无法建立健康检查、计数、回滚和降级通道，先保持简单路径通常更好。

### 证据与验收

每项选择都要有成功指标和反证条件：例如 XDP drop 的 drop counter 与 CPU 对照、DPDK 的固定包长 pps/p99、RDMA 的 CQE/error 与 RNIC 统计、offload 的 in_hw/命中计数。

## 2.3 一个可执行的决策序列

~~~
1. 明确 SLO：pps、Gbps、p99、CPU、隔离或可观测性。
2. 建立当前路径基线：环境、流量、CPU、队列、错误和计数。
3. 定位主成本：协议栈、copy、锁、NUMA、远端等待或控制面。
4. 选择最小机制，并写出新资源所有权。
5. 定义回退路径：不能 offload/设备故障/队列满时怎么办。
6. 在相同流量与环境中做对照，并记录证据等级。
~~~

第 4 步如果不能说明 buffer、queue 和完成 owner，说明方案尚不能进入实现。

## 2.4 反模式

- 以 kernel bypass 代替性能分析；
- 在 pcap、veth 或 RXE 上给出真实硬件性能承诺；
- 把 tc 规则存在当成硬件 offload 已命中；
- 把 CQE 当作远端业务持久化确认；
- 在没有幂等设计时，用超时重发处理 RDMA 写入；
- 先扩多队列/多 worker，再定义队列和 slot 所有权。

下一篇：[03：队列、内存与完成语义](03_QUEUES_MEMORY_AND_COMPLETION_SEMANTICS.md)。
