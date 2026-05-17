# 01_GOAL_AND_SCOPE

## 目标

本 lab 用 bpftrace 观测 Linux NAPI poll 路径，形成可复盘证据：

- `napi_poll` 是否可被 kprobe 观测
- `napi_poll` 返回值是否可被 kretprobe 观测
- `NET_RX` softirq 与 NAPI poll 是否能关联观察
- 驱动 poll 函数是否存在可观测入口
- CPU 分布、调用次数、返回值分布是否能输出

## 非目标

本 lab 不做：

- AF_XDP 收包
- XDP redirect
- libbpf ringbuf 工具
- 长时间性能分析

这些会放到后续 lab/project。
