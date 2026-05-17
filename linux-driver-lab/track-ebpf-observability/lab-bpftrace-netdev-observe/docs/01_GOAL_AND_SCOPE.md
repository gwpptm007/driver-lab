# 01_GOAL_AND_SCOPE

## 本 Lab 目标

用 bpftrace 做网络路径快速观测。

## 范围内

```text
bpftrace 环境检查
kprobe/tracepoint 可用性检查
netif_receive_skb 计数
napi_poll 计数
dev_queue_xmit 计数
softirq entry/exit 计数
records/review bundle
```

## 范围外

```text
不写 libbpf C 程序
不做长期 daemon
不修改包内容
不做高性能统计工具
```
