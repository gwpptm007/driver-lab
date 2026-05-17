# 03_PROBE_POINTS

## RX 主路径

```text
tracepoint:net:netif_receive_skb
```

## TX 主路径

```text
tracepoint:net:net_dev_queue
tracepoint:net:net_dev_xmit
```

## softirq

```text
tracepoint:irq:softirq_entry
tracepoint:irq:softirq_exit
```

常用向量：

```text
NET_TX_SOFTIRQ = 2
NET_RX_SOFTIRQ = 3
```

## optional kprobe

```text
kprobe:napi_poll
kprobe:netif_receive_skb
kprobe:dev_queue_xmit
```

这些 probe 点受内核符号、BTF、notrace 标记影响，不作为本 lab 的硬性验收条件。
