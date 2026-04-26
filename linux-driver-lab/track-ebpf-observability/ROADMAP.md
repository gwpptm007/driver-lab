# ROADMAP

## 总体顺序

1. `lab-bpftrace-netdev-observe`
2. `lab-kprobe-trace-napi-poll`
3. `lab-tracepoint-skb-path`
4. `lab-libbpf-net-observer`
5. `project-linux-network-observability`



## Phase 1: `lab-bpftrace-netdev-observe`

目标：

用 bpftrace 观察 netif_receive_skb、dev_queue_xmit、NAPI、softirq。

验收：

- 有 records
- 有 report
- 有 review bundle
- 能讲清和前一阶段的关系



## Phase 2: `lab-kprobe-trace-napi-poll`

目标：

系统化观察 NAPI poll、队列、CPU 分布。

验收：

- 有 records
- 有 report
- 有 review bundle
- 能讲清和前一阶段的关系



## Phase 3: `lab-tracepoint-skb-path`

目标：

使用 net tracepoints 观察 receive/xmit。

验收：

- 有 records
- 有 report
- 有 review bundle
- 能讲清和前一阶段的关系



## Phase 4: `lab-libbpf-net-observer`

目标：

开始写 C/libbpf 小工具。

验收：

- 有 records
- 有 report
- 有 review bundle
- 能讲清和前一阶段的关系



## Phase 5: `project-linux-network-observability`

目标：

收成 per-interface/per-CPU/path report 工具项目。

验收：

- 有 records
- 有 report
- 有 review bundle
- 能讲清和前一阶段的关系
