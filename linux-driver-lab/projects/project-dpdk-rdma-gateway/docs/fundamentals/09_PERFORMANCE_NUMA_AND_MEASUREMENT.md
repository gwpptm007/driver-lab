# 09. 性能、NUMA 与测量方法

当前 pcap PMD + RXE 环境证明功能闭环，不提供 RNIC 吞吐或延迟结论。性能工作应从可复现的测量协议开始，而不是从单次 `pps` 输出开始。

## 9.1 需要同时观察的指标

| 类别 | 指标 | 解释 |
| --- | --- | --- |
| 输入 | RX pps、包大小分布、UDP 接受率 | 流量是否稳定且可比较 |
| 端到端 | accepted pps、完成 pps、p50/p99/p999 延迟 | 结果而非单点吞吐 |
| 队列 | ring occupancy、最大值、ring_full | worker 是否跟不上 producer |
| slot | FREE/READY/INFLIGHT 数、高水位、slot_exhausted | 是否被远端完成速度限制 |
| RDMA | post rate、outstanding WR、CQ poll batch、CQE error | QP/CQ 是否是瓶颈 |
| 系统 | 每核 CPU、软中断、NUMA remote access、内存带宽 | 是否出现隐藏的系统瓶颈 |

计数必须同时展示时间窗口与累计值。仅有累计 `completed` 无法判断暂停、尾延迟或短暂的 ring 满。

## 9.2 NUMA 的数据路径原则

真实硬件上，NIC/RDMA device 的 PCIe NUMA node、RX lcore、mempool、staging memory、RDMA worker 和其 CQ/QP 最好位于同一 socket。跨 socket 会带来远端内存访问与 PCIe 访问成本，且常在低负载时不明显、高负载时放大。

建议在实验记录中固定并报告：设备 BDF 与 NUMA node、CPU affinity、hugepage node、mempool node、RDMA device 名称、IRQ/队列分配。当前项目不以 RXE 环境声称已经完成这些绑定验证。

## 9.3 基准测试的最小协议

1. 固定代码 revision、DPDK/rdma-core/内核版本、NIC firmware 和 CPU governor；
2. 固定包大小、流数、协议混合比例、远端 MR 大小与持续时间；
3. 固定 lcore/worker 亲和性与 hugepage/NUMA 配置；
4. 先 warm-up，再测量多个独立轮次；
5. 每次只改变一个变量，并同时保存原始计数与环境元数据；
6. 报告中区分 pcap/RXE 功能测试与真实 NIC/RNIC 压测。

若没有这些信息，“更快”通常无法被复现，也无法解释。

## 9.4 batch 与 selective signaling 的正确性门槛

批量 post WR 可降低 doorbell 和函数调用开销；selective signaling 可降低 CQE 产生率。但它们改变了完成归属模型：一个 signaled completion 可能表示此前一组 WR 已按队列顺序完成，前提是 QP 语义、错误处理与批次边界都被明确建模。

实施前至少需要定义：批次元数据、每批第一个/最后一个 `wr_id`、最大 outstanding WR、何时强制 signaled、错误 CQE 覆盖范围、关闭时如何 flush 未 signaled 请求。未建立这套模型前，每 slot 一个可追踪完成虽然更慢，却更可信。

## 9.5 分阶段验收

| 阶段 | 可接受结论 |
| --- | --- |
| pcap + RXE | 协议、生命周期、计数守恒与错误路径可验证 |
| 单机真实 NIC/RNIC | 设备绑定、线速附近吞吐、CQ/slot 背压曲线 |
| 双机真实网络 | MTU、拥塞、链路抖动与跨主机尾延迟 |
| 长稳压测 | 内存泄漏、slot 泄漏、错误恢复和指标稳定性 |

每一阶段都应附带上一阶段的正确性断言，不能为吞吐而取消 generation、边界检查或 drain 验证。

延伸阅读：[DPDK Ring Library](https://doc.dpdk.org/guides/prog_guide/ring_lib.html)。
