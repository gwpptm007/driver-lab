# 03_OBSERVABILITY_POINTS

## 第一批观测点

| 类型 | 观测点 | 作用 |
|---|---|---|
| kprobe | `netif_receive_skb` | 观察 skb 进入协议栈附近的 RX 路径 |
| kprobe | `dev_queue_xmit` | 观察 TX 进入 qdisc/dev 层路径 |
| kprobe | `napi_poll` | 观察 NAPI poll 调度 |
| tracepoint | `irq:softirq_entry` | 观察软中断入口 |
| tracepoint | `irq:softirq_exit` | 观察软中断退出 |

## 注意

不同内核版本、编译选项、发行版会影响 kprobe 可用性。
所以本 track 采用：

```text
先列出 probe 点
再执行 best-effort bpftrace
最后在 REVIEW_BUNDLE 中记录哪些可用、哪些不可用
```
