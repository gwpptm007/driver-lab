# 03_NAPI_OBSERVE_POINTS

## 主要观测点

| 类型 | 观测点 | 说明 |
|---|---|---|
| kprobe | `napi_poll` | NAPI 核心 poll 调度函数 |
| kretprobe | `napi_poll` | 返回值分布，近似观察本轮 poll 处理量 |
| kprobe | `*poll*` driver symbols | 尝试发现驱动 poll 函数 |
| tracepoint | `irq:softirq_entry/exit` | 观察 NET_RX softirq |

## 为什么保留 tracepoint fallback

kprobe 依赖函数符号，受内核版本、编译选项、notrace、安全策略影响。tracepoint ABI 更稳定。工程实践中通常：

```text
tracepoint 做稳定基线
kprobe 做细节补充
```
