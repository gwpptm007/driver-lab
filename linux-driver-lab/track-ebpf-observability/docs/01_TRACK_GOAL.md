# 01_TRACK_GOAL

## 目标

`track-ebpf-observability` 的目标是补齐“看得见内核网络路径”的能力。

前面的主线偏数据面实现：

```text
netdev / virtio / DPDK / AF_XDP
```

这条主线偏问题定位和可观测：

```text
bpftrace / kprobe / tracepoint / libbpf / ringbuf
```

最终你应该能解释：

```text
1. 包进入内核协议栈前后能在哪里观测
2. NAPI poll、softirq 与网卡收包之间是什么关系
3. TX 什么时候进入 dev_queue_xmit
4. 为什么先用 bpftrace 快速验证，再用 libbpf 工具化
5. 现场问题如何用 eBPF 低风险定位
```
