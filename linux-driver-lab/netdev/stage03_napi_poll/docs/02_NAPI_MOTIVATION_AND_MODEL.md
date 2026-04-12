# 02_NAPI_MOTIVATION_AND_MODEL

## 为什么需要 NAPI

如果每收到一帧都立刻触发一次硬中断，然后在中断里做完整 RX 处理，流量一高时 CPU 会被中断打爆。

NAPI 的核心动机不是“多一种写法”，而是：

> **把“每包一中断”变成“中断只负责通知，真正处理交给 poll 批量完成”。**

## stage03 的教学型抽象

真实网卡通常是：
- 设备把完成的 descriptor 放进 RX ring
- 触发一次中断
- 驱动在 irq 里 mask 中断并 `napi_schedule`
- poll 批量 drain ring
- queue 处理干净后 complete，再 re-enable irq

stage03 没有真实硬件，所以把这件事抽象成：

- `pending_rxq` 代替硬件 RX ring
- `stage03_raise_irq()` 代替硬件 irq
- `napi_schedule_prep() / __napi_schedule()` 代替真实 irq handler 中的 schedule
- `stage03_napi_poll()` 代替 poll handler

## direct 与 napi 的差异

### direct 模式
```text
start_xmit
  -> build rx skb
  -> netif_rx
```

特点：
- 路径短
- 便于理解 `skb`
- 但没有批处理语义

### napi 模式
```text
start_xmit
  -> build rx skb
  -> enqueue pending_rxq
  -> raise irq
  -> napi schedule
  -> poll drain queue
  -> netif_receive_skb
```

特点：
- 多了一层排队与批处理
- 能观察 budget / complete / queue depth
- 更接近真实网络驱动

## 为什么 poll 里用 `netif_receive_skb()`

stage02 里没有 NAPI，所以用了 `netif_rx()`。

到了 stage03：
- RX 已经在 poll 上下文里
- 不需要再通过 `NET_RX_SOFTIRQ` 转一层
- 直接 `netif_receive_skb()` 更符合 NAPI 驱动语义

这也是 stage02 → stage03 最关键的语义变化之一。
