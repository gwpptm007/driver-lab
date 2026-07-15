# 06. 可观测性与故障定位手册

高性能路径的故障常表现为很少几个字：慢、丢、错、没 offload。排查时不要从某个工具或某条 log 开始，而要沿数据路径逐段建立守恒关系。

## 6.1 先定义可守恒的计数

以 packet forwarding 为例：

~~~
rx = parsed + malformed + unsupported
parsed = forwarded + policy_drop + local_enqueue_failure
forwarded = tx_accepted + tx_failure
~~~

以异步 RDMA 为例：

~~~
posted = successful_cqe + failed_cqe + outstanding
active_buffers = ready + inflight
~~~

计数不一定永远完全相等，但差值必须可解释。它比单个成功日志更早暴露 buffer 泄漏、重复完成、队列满和错误路径遗漏。

## 6.2 四步定位法

### 第一步：确认输入

检查物理端口/virtio/veth 统计、RX queue、pcap 输入或应用发送端。若输入就不稳定，后续所有 latency/throughput 分析都无效。

### 第二步：确认路径选择

确认包进入了预期队列、XDP program、AF_XDP socket、DPDK port、RDMA QP 或 representor。配置存在不能替代命中证据。

### 第三步：确认资源压力

观察 ring occupancy、mempool/UMEM 可用 frame、TX descriptor、SQ/CQ 深度、slot 状态和 CPU run queue。高水位、drop 与 p99 应在同一时间窗关联分析。

### 第四步：确认完成与远端

查看 TX reclaim/CQE、QP/port error、远端计数和业务确认。对于 RDMA WRITE，local CQE 与远端应用处理是两个独立事件。

## 6.3 工具的分工

| 工具类别 | 擅长回答 | 常见误用 |
| --- | --- | --- |
| 应用 metrics | 业务、队列、buffer 与错误守恒 | 没有环境标签，无法比较 |
| ethtool / driver stats | port、queue、硬件错误 | 只看总数，不对齐时间窗 |
| tc / devlink | rule offload、eSwitch、health、资源 | 将 command success 当成硬件执行 |
| eBPF trace/metrics | kernel hook、drop、延迟样本、调用路径 | 全量高频事件造成额外负载 |
| perf / CPU profile | CPU 热点、cache、调度 | 在不同流量和亲和性下直接比较 |
| RDMA verbs/CQ 统计 | WR、CQE、QP 状态 | 只看 post 成功，不看异步失败 |

先用低开销计数定位阶段，再用 trace 或 profile 钻入热点；不要长期用逐包 printk 或同步日志观测数据面。

## 6.4 未 offload 的诊断

当规则看似存在却没有性能变化，应依次检查：

1. NIC、driver、firmware、eswitch mode 与 representor 是否符合前置条件；
2. rule 是否被接受为 in_hw，是否出现软件 fallback；
3. 测试流量的协议/字段/方向是否精确匹配；
4. rule counter、port counter 和 host CPU 是否同时变化；
5. devlink health、资源水位或 driver 日志是否显示拒绝/回退原因。

除非有这些证据，不应把规则描述成硬件 offload。

## 6.5 最小事件字段

异常样本至少包括时间、路径阶段、接口/queue/QP、流量或 request 标识、长度、配置版本、错误码/状态、NUMA/worker 标识。payload 默认不进入日志；需要采样时应脱敏并有速率限制。

下一篇：[07：SmartNIC、DPU 与 representor](07_SMARTNIC_DPU_REPRESENTOR_AND_OFFLOAD.md)。
