# track-ebpf-observability

> eBPF 网络可观测性主线。

## 一句话定位

把前面 `netdev / DPDK / AF_XDP` 学到的数据路径，转换成可观测、可定位、可复盘的 eBPF 网络观测能力。

这条 track 不优先追求修改数据面，而是回答几个工程问题：

```text
包从哪里进来？
在哪个函数/tracepoint 能看到？
NAPI/softirq 有没有跑？
TX 路径有没有进入 dev_queue_xmit？
drop 或异常应该怎么定位？
最后如何沉淀成 libbpf 小工具？
```

## 阶段列表

| 阶段 | 目录 | 状态 | 目标 |
|---|---|---|---|
| Phase 1 | `lab-bpftrace-netdev-observe` | `READY_TO_TEST` | 用 bpftrace 快速观测 RX/TX/NAPI/softirq |
| Phase 2 | `lab-kprobe-trace-napi-poll` | `PLANNED` | 专门观测 NAPI poll、CPU 分布、预算关系 |
| Phase 3 | `lab-tracepoint-skb-path` | `PLANNED` | 使用 tracepoint 观察 skb 路径 |
| Phase 4 | `lab-libbpf-net-observer` | `PLANNED` | 从 bpftrace 迁移到 C/libbpf 工具 |
| Phase 5 | `project-linux-network-observability` | `PLANNED` | 收口成网络路径观测项目 |

## 当前入口

```bash
cd track-ebpf-observability/lab-bpftrace-netdev-observe
cat START_HERE.md
```

## 目录原则

`track-ebpf-observability/` 根目录只放导航文档和各个 lab/project 目录，不直接放通用 `records/ reports/ scripts/`。
每个 lab/project 自己维护自己的：

```text
docs/
scripts/
records/
reports/
```
