# STAGE_OVERVIEW

## stage02 目标与边界

把下面这条教学路径真正跑起来：

```
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

---

## 为什么 stage02 不直接谈 ring / DMA

因为 `ring / DMA` 回答的是"怎么搬运包"。但在你真正理解：
- 包是什么
- 包进入/离开驱动的边界在哪里
- TX / RX 分别在哪一层完成交接

之前，过早进入搬运机制，反而会让概念倒置。

> **先理解处理对象，再理解搬运机制。**

---

## skb 是什么

`struct sk_buff` 不止是 buffer，是承载网络数据的"对象"。它至少承载：
- 数据区：head / data / tail / end 指针
- 元数据：protocol、dev、pkt_type、len、ip_summed 等

---

## skb_clone vs skb_copy

### skb_copy()
- 分配全新的 struct sk_buff
- 分配全新的数据区（memcpy 原数据）
- 两份独立的数据，互不影响
- 成本高，但语义直观

### skb_clone()
- 分配全新的 struct sk_buff（头）
- 共享原始数据区（引用计数 +1）
- 成本低，但要理解"头共享 / 数据共享 / 生命周期"

---

## ndo_start_xmit() 里都做了什么

1. 更新 TX 统计
2. 根据 `loop_mode` 创建一份 RX skb
3. 清理/修正 RX 注入所需元数据
4. 调用 `eth_type_trans()` 为 RX 路径设置 protocol
5. 调用 `netif_rx()` 把包送回协议栈
6. 更新 RX/注入统计
7. 消费原始 TX skb

---

## 为什么这里用 netif_rx()

因为 stage02 还没有 NAPI，也没有驱动侧 poll。`netif_rx()` 很适合作为教学入口：
- 明确表示"把 skb 重新交给协议栈接收路径"
- 代码直观
- 方便后面 stage03 再对照 NAPI 的收包方式

---

## 与 stage03 的关系

- **stage03**：NAPI / poll / 中断抑制
- **stage04**：ring / DMA / RX replenishment

```
stage01: 看见包到了
stage02: 环回包（skb clone/copy + netif_rx）
stage03: NAPI 批处理
stage04: ring / DMA / refill
```
