# eBPF Observability Fundamentals 学习入口

这套文档放在五个观测 Phase 之前。目标不是背 bpftrace 单行命令，而是理解：事件从哪个 hook 进入 BPF、verifier 如何证明安全、map 如何保存跨事件状态、事件如何送回用户态，以及如何把多个观测点关联成一条可信网络路径。

## 学完后应能回答

1. eBPF program 从 ELF 到 verifier、JIT、attach、detach 经历什么？
2. tracepoint、raw tracepoint、kprobe、fentry/fexit、XDP、tc 各适合什么问题？
3. verifier 为什么拒绝未界定指针、无界循环和过大栈？
4. hash、array、per-CPU、LRU、ring buffer 等 map 如何选择？
5. bpftrace 的快速探索结果怎样迁移到稳定 libbpf 工具？
6. kprobe 为什么易受符号、内联和内核版本影响？
7. tracepoint 为什么更稳定，又为什么字段仍需按目标内核检查？
8. BTF、CO-RE、`vmlinux.h`、skeleton 分别解决什么问题？
9. ringbuf 和 perfbuf 的 ordering、backpressure、丢事件边界是什么？
10. 如何关联 softirq、NAPI、skb RX/TX/drop 而不过度推断因果？
11. 如何控制 probe 开销、采样率、map cardinality 和日志风暴？
12. 遇到 attach 失败、计数为零、事件丢失时怎样分层排查？

## 推荐阅读顺序

| 顺序 | 文档 | 核心模型 |
| --- | --- | --- |
| 1 | [00_15_MINUTE_MENTAL_MODEL.md](00_15_MINUTE_MENTAL_MODEL.md) | 15 分钟建立全局轮廓 |
| 2 | [01_EBPF_KERNEL_ARCHITECTURE.md](01_EBPF_KERNEL_ARCHITECTURE.md) | syscall、verifier、JIT、link、map |
| 3 | [02_PROGRAM_TYPES_AND_HOOK_SELECTION.md](02_PROGRAM_TYPES_AND_HOOK_SELECTION.md) | program type 与 hook 选型 |
| 4 | [03_VERIFIER_MEMORY_AND_SAFETY.md](03_VERIFIER_MEMORY_AND_SAFETY.md) | verifier 抽象解释与内存安全 |
| 5 | [04_MAPS_STATE_AND_CONCURRENCY.md](04_MAPS_STATE_AND_CONCURRENCY.md) | map、并发、生命周期、基数 |
| 6 | [05_BPFTRACE_EXPLORATION_WORKFLOW.md](05_BPFTRACE_EXPLORATION_WORKFLOW.md) | 快速探索与证据边界 |
| 7 | [06_KPROBE_FENTRY_AND_FUNCTION_TRACING.md](06_KPROBE_FENTRY_AND_FUNCTION_TRACING.md) | 动态函数探针及稳定性 |
| 8 | [07_TRACEPOINTS_AND_STABLE_EVENTS.md](07_TRACEPOINTS_AND_STABLE_EVENTS.md) | tracepoint/raw tracepoint/schema |
| 9 | [08_BTF_CORE_LIBBPF_AND_SKELETON.md](08_BTF_CORE_LIBBPF_AND_SKELETON.md) | 可移植 libbpf 工程链 |
| 10 | [09_RINGBUF_PERFBUF_AND_EVENT_TRANSPORT.md](09_RINGBUF_PERFBUF_AND_EVENT_TRANSPORT.md) | 内核到用户态事件通道 |
| 11 | [10_NETWORK_PATH_CORRELATION.md](10_NETWORK_PATH_CORRELATION.md) | softirq/NAPI/skb 路径关联 |
| 12 | [11_OVERHEAD_SAMPLING_AND_PRODUCTION.md](11_OVERHEAD_SAMPLING_AND_PRODUCTION.md) | 开销、采样与生产安全 |
| 13 | [12_DEBUGGING_PROJECT_MAP_AND_RECALL.md](12_DEBUGGING_PROJECT_MAP_AND_RECALL.md) | 排障、项目映射和速记 |
| 14 | [13_STACKS_SYMBOLIZATION_AND_FLAMEGRAPHS.md](13_STACKS_SYMBOLIZATION_AND_FLAMEGRAPHS.md) | 栈、符号化和火焰图 |
| 15 | [14_SECURITY_CAPABILITIES_AND_CONTAINERS.md](14_SECURITY_CAPABILITIES_AND_CONTAINERS.md) | 权限、容器和安全边界 |

## 五条贯穿主线

```mermaid
flowchart LR
    H[Hook 选择] --> V[Verifier/JIT]
    V --> S[Map state]
    S --> E[Event transport]
    E --> U[Userspace aggregation]
    U --> C[Correlation/conclusion]
```

## 结论边界

- “probe 命中”只证明该 hook 在当前负载出现，不自动证明完整调用链。
- 计数为零可能是无流量、hook 不存在、过滤错误、CPU 分布变化或 attach 失败。
- kprobe 可用于探索内部实现，长期工具优先 tracepoint、fentry/CO-RE 或稳定接口。
- eBPF 降低观测侵入性，但不是零开销；高频 hook 上逐事件输出可能严重扰动目标系统。

原有 [../03_OBSERVABILITY_POINTS.md](../03_OBSERVABILITY_POINTS.md) 和各 lab 文档继续保留；本目录是新的统一前置入口。执行入口见 [../../START_HERE.md](../../START_HERE.md)。

