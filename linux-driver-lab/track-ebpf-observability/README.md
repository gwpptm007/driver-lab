# track-ebpf-observability

> eBPF 网络可观测性主线

## 一句话定位

把之前零散 trace/stats/tcpdump/ftrace 能力系统化成 bpftrace、kprobe、tracepoint、libbpf 观测项目。

## 阶段列表

1. `lab-bpftrace-netdev-observe` — bpftrace 快速网络观测
2. `lab-kprobe-trace-napi-poll` — kprobe 观测 NAPI poll
3. `lab-tracepoint-skb-path` — tracepoint 观测 skb path
4. `lab-libbpf-net-observer` — libbpf 网络观测工具
5. `project-linux-network-observability` — Linux 网络可观测性项目

## 推荐方式

按 `ROADMAP.md` 顺序推进，每个 Lab 都要形成：

- README
- START_HERE
- docs
- scripts
- records
- reports
- acceptance
