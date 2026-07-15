# 10. 可观测性、证据与回归

可观测性不是在异常发生后补一行日志，而是让每个阶段的守恒关系可以持续检查。这个项目的尺寸很小，正适合先建立严格证据链，再把模式扩展到真实设备和更大并发。

## 10.1 四层指标

| 层 | 关键指标 | 主要问题 |
| --- | --- | --- |
| ingress | rx、UDP、unsupported、malformed、staged | 包为什么没有进入数据面？ |
| handoff | enqueue/dequeue、ring occupancy、ring_full | producer 和 worker 是否失衡？ |
| slot | FREE/READY/INFLIGHT、generation mismatch、slot_exhausted | payload 所有权是否闭合？ |
| RDMA | post、CQE 成功/失败、bytes、QP 状态 | 传输与完成是否可信？ |

计数名应反映边界，而不是实现细节。比如 `completed` 必须明确是“收到本地成功 CQE 并匹配 generation”，而不是“调用过 post_send”。

## 10.2 当前 Phase 4 的守恒证据

pcap 基线的 64 个输入包给出如下可自动验证关系：

```text
rx = udp + unsupported + malformed = 64
udp = staged = dequeued = completed = 48
payload_bytes = 48 * 32 = 1536
wire_write_bytes = 48 * (40 + 32) = 3456
active_slots_after_drain = 0
```

一条等式失败时，应从对应边界排查：第一行说明解析分类；第二行说明入队与 worker；字节行说明 ABI/wire 编码；最后一行说明完成与退出。不要只以进程退出码为“成功”证据。

## 10.3 日志、指标与 trace 的分工

- **指标**：每个数据面事件的低成本累计和时间窗口统计；
- **结构化错误日志**：只记录状态变化、失败上下文与限速后的样本；
- **trace/采样**：用于还原少量 request 的跨边界时间线，不承载全量吞吐；
- **测试输出**：保留输入、断言、版本和环境信息，成为可审计证据。

成功路径逐包打印会改变调度、缓存和尾延迟，因此不能把 debug log 打开后的性能当作基准。未来接入 eBPF 时也应先定义采样率、开销预算与数据脱敏边界，观察数据面而不是改变其时序。

## 10.4 回归的分层组织

| 层次 | 应验证的内容 |
| --- | --- |
| contract unit | descriptor 大小、字节序、slot 状态、ring 回绕、generation |
| ingress unit | IPv4 IHL、UDP 长度、跨 segment 读取、mbuf 归还、统计分类 |
| RDMA integration | RC 建连、MR 边界、成功/失败 CQE、远端内容校验 |
| end-to-end | pcap 64 包基线、48 次写入、drain 后 slot 归零 |
| 长稳与故障 | ring/slot 压力、停止、QP error、资源回收（未来阶段） |

每新增性能特性都应保留前四层测试。批量化不能跳过 stale completion 测试，多队列不能跳过单队列的关闭语义。

## 10.5 一次结果应携带什么

建议每份测试/压测结果记录：git revision、构建选项、DPDK/rdma-core/内核版本、pcap 文件及其 hash、RXE 或 NIC/RNIC 信息、CPU 亲和性、NUMA/hugepage 配置、命令行参数、原始计数、失败样本和时间范围。这使“通过”从一段终端文本变成可复查的实验记录。

相关入口：[`tests/TEST_FLOW.md`](../../tests/TEST_FLOW.md)、[`README.md`](../../README.md)。
