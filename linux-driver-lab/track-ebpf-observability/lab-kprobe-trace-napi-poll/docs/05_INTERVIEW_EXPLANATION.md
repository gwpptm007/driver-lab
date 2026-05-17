# 05_INTERVIEW_EXPLANATION

面试中可以这样描述这一站：

```text
我用 bpftrace 对 Linux NAPI poll 做了专项观测，先通过 tracepoint 确认 NET_RX softirq，再用 kprobe/kretprobe 观察 napi_poll 的调用频率、CPU 分布和返回值分布。考虑到 kprobe 受内核符号和 notrace 限制，我把驱动 poll 函数观测设计成 optional，并保留 tracepoint fallback，这更接近真实线上排障方式。
```

可展开点：

- NAPI 是中断缓解和批量收包机制
- softirq 是 NAPI poll 的执行上下文之一
- kprobe 适合临时深入，但稳定性不如 tracepoint
- 观测工具不能只看能不能 attach，还要看数据是否非零、是否与接口统计一致
