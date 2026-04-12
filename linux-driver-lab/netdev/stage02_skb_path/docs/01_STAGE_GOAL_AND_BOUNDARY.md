# 01. stage02 目标与边界

## 1. stage02 的核心目标

把下面这条教学路径真正跑起来：

```text
userspace sender
    -> AF_PACKET sendto()
    -> 内核网络栈
    -> ndo_start_xmit()
    -> 驱动构造 RX skb（copy 或 clone）
    -> netif_rx()
    -> 协议栈 RX 路径
    -> userspace receiver
```

这个阶段最重要的不是性能，而是把 **`skb` 是 TX/RX 共同核心对象** 这个事实吃透。

## 2. 为什么 stage02 不直接谈 ring / DMA

因为 `ring / DMA` 回答的是“怎么搬运包”。
但在你真正理解：
- 包是什么
- 包进入/离开驱动的边界在哪里
- TX / RX 分别在哪一层完成交接

之前，过早进入搬运机制，反而会让概念倒置。

所以 stage02 的正确定位是：

> **先理解处理对象，再理解搬运机制。**

## 3. 本阶段明确不做的事

### 不做 NAPI
NAPI 是 stage03 的主题，那里才讨论：
- 为什么不用每包一个硬中断
- poll / budget 是什么
- 中断抑制与重开语义

### 不做 ring / descriptor
ring 是 stage04 的主题，那里才讨论：
- ownership
- producer / consumer
- refill / 枯竭

### 不做 DMA
DMA 同样留在 stage04，stage02 先做软件注入闭环。

## 4. 本阶段的“教学模型”是什么

这里的 `netdev_stage02` 并不模拟真实网卡硬件。
它做的是：
- 在 TX 入口抓到一帧 `skb`
- 用 copy 或 clone 的方式构造“新的 RX skb”
- 再用 `netif_rx()` 把它送回 RX 路径

这是一个 **software loopback injection model**，适合作为 stage02 的教学模型。
