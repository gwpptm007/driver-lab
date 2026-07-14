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

## 第一次进入先读这里

先打开 [docs/fundamentals/README.md](docs/fundamentals/README.md)，按“内核架构 -> hook 选型 -> verifier -> maps -> bpftrace -> tracepoint/fentry -> CO-RE -> event transport -> 路径关联 -> 生产安全”建立完整模型，再进入 Phase 1。00-02 章同时提供 [GIF、PNG 与可单步操作的 Canvas 视觉层](docs/fundamentals/visuals/README.md)，适合先恢复路径感，再追源码细节。

知识层包含 15 个主题，状态：`EBPF_OBSERVABILITY_FUNDAMENTALS_COMPLETE`。

## 阶段列表

| 阶段 | 目录 | 状态 | 目标 |
|---|---|---|---|
| Phase 0 | `docs/fundamentals/` | `COMPLETED` | eBPF 可观测性原理、图示、排障与项目映射 |
| Phase 1 | `lab-bpftrace-netdev-observe` | `COMPLETED` | 用 bpftrace 快速观测 RX/TX/NAPI/softirq |
| Phase 2 | `lab-kprobe-trace-napi-poll` | `COMPLETED` | 专门观测 NAPI poll、CPU 分布、预算关系 |
| Phase 3 | `lab-tracepoint-skb-path` | `COMPLETED` | 使用 tracepoint 观察 skb 路径 |
| Phase 4 | `lab-libbpf-net-observer` | `COMPLETED` | 从 bpftrace 迁移到 C/libbpf 工具 |
| Phase 5 | `project-linux-network-observability` | `COMPLETED` | 收口成网络路径观测项目 |

## 当前入口

**全部 5 个 Phase 已完成（2026-06-07）**：

| Phase | 判定 | 日期 |
|-------|------|------|
| Phase 1 | PASS_BPFTRACE_NETDEV_OBSERVE | 2026-06-07 |
| Phase 2 | PASS_NAPI_POLL_OBSERVE | 2026-05-18 |
| Phase 3 | PASS_SKB_TRACEPOINT_OBSERVE | 2026-06-06 |
| Phase 4 | PASS_LIBBPF_OBSERVER | 2026-06-06 |
| Phase 5 | PASS_PROJECT_NET_OBSERVABILITY | 2026-06-06 |

```bash
cat track-ebpf-observability/ROADMAP.md
```

知识层与测试记录：

```text
docs/fundamentals/README.md
tests/TEST_RECORD_20260714_EBPF_FUNDAMENTALS.md
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
