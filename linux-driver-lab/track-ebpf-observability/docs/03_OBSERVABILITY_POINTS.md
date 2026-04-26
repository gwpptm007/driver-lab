# 03_OBSERVABILITY_POINTS

## 推荐观测点

- `netif_receive_skb`
- `dev_queue_xmit`
- NAPI poll
- softirq
- `net:netif_receive_skb`
- `net:net_dev_queue`
- `net:net_dev_xmit`

## 原则

先 bpftrace 快速验证，再 kprobe/tracepoint 深化，最后 libbpf 工具化。
