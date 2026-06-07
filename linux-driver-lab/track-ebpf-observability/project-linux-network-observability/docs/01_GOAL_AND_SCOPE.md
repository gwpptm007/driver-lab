# 01_GOAL_AND_SCOPE — 原理篇

## 1. 项目目标

将 Phase 1-4 的 eBPF 网络可观测性学习成果整合为一个**可复用的工程工具**，具备三个维度的观测能力：

```text
1. per-interface  — 按网卡 (ens33/lo/ens34) 统计 RX/TX/DROP 流量
2. per-CPU        — 展示软中断/NAPI poll 的 CPU 负载分布
3. path report    — RX->GRO, TX-QUEUE->TX-XMIT 不变量验证 + DROP 原因分类
```

## 2. 整体架构

```text
                         Linux Kernel
                   5 Kernel Tracepoints (ABI)
                  netif_receive_skb, napi_gro_receive_entry,
                  net_dev_queue, net_dev_start_xmit, kfree_skb
                              |
                    BPF Programs (net_observer.bpf.c)
                     submit_event() + event_counts map
                         /              \
                  ringbuf (events)    bpf_map_lookup (counts)
                       |                    |
              Userspace (net_observer.c)    |
             handle_event()                |
             per-interface 聚合             |
                    \                      /
                 generate_report()
                 -> 控制台 + Markdown 文件
```

## 3. 与 Phase 1-4 的区别

| 维度 | Phase 4 | Phase 5 |
|------|---------|---------|
| per-interface 统计 | ifname 仅显示在事件中 | 聚合为结构化表格 |
| per-CPU 分布 | 仅在事件中有 cpu= 字段 | 独立统计表 (event_counts map) |
| DROP 原因 | 无 | 从 kfree_skb reason 字段提取 |
| protocol 字段 | 无 | 从 kfree_skb protocol 字段提取 |
| 输出格式 | 控制台 printf() | 控制台 + Markdown 报告文件 |
| CLI 参数 | -d -v | -d -v -o -i |

## 4. 关键技术设计

### 4.1 per-interface 统计 (userspace 聚合)

设计决策: 在 userspace 做 per-interface 聚合，而非 BPF。
- 优点: 灵活、易扩展、不增加 BPF 指令复杂度
- 适用条件: 事件速率低 (<10K events/s)，内存开销可忽略

```c
#define MAX_IFACE_STATS 32
struct iface_stats {
    char  name[16];
    __u64 rx_pkts, rx_bytes;
    __u64 gro_events;
    __u64 tx_queue_pkts, tx_queue_bytes;
    __u64 tx_xmit_pkts, tx_xmit_bytes;
    __u64 drop_pkts;
    __u64 drop_reasons[64];  // 按 reason 分类
};
```

### 4.2 per-CPU 分布 (BPF PERCPU_ARRAY 读取)

BPF 端: `BPF_MAP_TYPE_PERCPU_ARRAY` keyed by event type (0-4), value = per-CPU u64.
Userspace: 分配 EVENT_MAX * ncpus 矩阵，读取每个 key 的 per-CPU 数组。

### 4.3 DROP 原因提取

kfree_skb tracepoint 提供 `reason` 字段 (offset:28, enum skb_drop_reason)。
Userspace 将枚举值翻译为可读名称:
NOT_SPECIFIED(2), NO_SOCKET(3), TCP_CSUM(5), NETFILTER_DROP(8), NOMEM(63) 等。

### 4.4 路径分析

- RX->GRO: NAPI 网卡 RX 包都经过 GRO (排除 loopback)
- TX-QUEUE->TX-XMIT: 入队 = 发送 (100%)
- DROP rate: drop / (rx + tx_queue)，正常流量为 0%

## 5. 报告格式

1. Per-Interface 统计: 表格展示每块网卡的事件数和字节数
2. Per-CPU 分布: 每个 CPU 的 5 种事件计数
3. 路径分析: 三个不变量的百分比和判定
4. 总结: 汇总表 + 状态文本
