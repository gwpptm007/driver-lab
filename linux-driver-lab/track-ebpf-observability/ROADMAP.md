# ROADMAP

## 总体顺序

```text
lab-bpftrace-netdev-observe
    ↓
lab-kprobe-trace-napi-poll
    ↓
lab-tracepoint-skb-path
    ↓
lab-libbpf-net-observer
    ↓
project-linux-network-observability
```

## Phase 1：`lab-bpftrace-netdev-observe`

目标：

```text
用 bpftrace 快速观察 Linux 网络 RX/TX/NAPI/softirq 路径。
```

验收：

```text
ENV_CHECK.txt 存在
PROBE_POINTS.txt 存在
RX_OBSERVE.log / TX_OBSERVE.log 至少一个存在
SOFTIRQ_OBSERVE.log / NAPI_OBSERVE.log 至少一个存在
REVIEW_BUNDLE.md 存在
能够解释 kprobe 与 tracepoint 的适用边界
```

状态：`READY_TO_TEST`

## Phase 2：`lab-kprobe-trace-napi-poll`

目标：

```text
专门围绕 NAPI poll 做观测，包括 napi_poll 调用次数、CPU 分布、软中断关系。
```

状态：`PLANNED`

## Phase 3：`lab-tracepoint-skb-path`

目标：

```text
使用内核 tracepoint 观察 skb receive/xmit/drop 路径，降低 kprobe 函数名变化带来的不稳定。
```

状态：`PLANNED`

## Phase 4：`lab-libbpf-net-observer`

目标：

```text
把前面 bpftrace 验证过的点迁移到 C/libbpf，使用 ringbuf/perfbuf 输出事件。
```

状态：`PLANNED`

## Phase 5：`project-linux-network-observability`

目标：

```text
形成一个项目型网络路径观测工具，能输出 per-interface / per-CPU / RX-TX path report。
```

状态：`PLANNED`
