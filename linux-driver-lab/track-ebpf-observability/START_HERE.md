# START_HERE

当前进入 eBPF 网络可观测性主线。

## 推荐阅读顺序

```text
1. README.md
2. ROADMAP.md
3. docs/01_TRACK_GOAL.md
4. lab-bpftrace-netdev-observe/START_HERE.md
```

## 第一站

```text
lab-bpftrace-netdev-observe
```

这一站先不写复杂 C/libbpf 程序，而是用 bpftrace 快速建立网络路径观测感：

```text
netif_receive_skb
napi_poll
dev_queue_xmit
irq:softirq_entry / irq:softirq_exit
```
