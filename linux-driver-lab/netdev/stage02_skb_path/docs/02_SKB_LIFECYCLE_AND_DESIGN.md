# 02. skb 生命周期与 stage02 设计

## 1. `skb` 是什么

`struct sk_buff` 不是“一个简单 buffer 指针”，而是 Linux 网络栈贯穿 TX/RX 的核心数据对象。

它至少承载三类信息：
- 数据区：以太头、L3/L4 数据、payload
- 边界信息：head / data / tail / end
- 元数据：protocol、dev、pkt_type、len、ip_summed 等

## 2. 为什么 stage02 要强调 `clone` 和 `copy`

因为这两种操作刚好能帮助你理解：

### `skb_copy()`
- 分配新的 skb 和新的数据区
- 数据真正复制一份
- 成本更高，但语义直观
- 适合讲“TX 抓到一帧后，我重新造一帧 RX 包”

### `skb_clone()`
- 新建 skb 头部
- 共享原数据区
- 引用计数增加
- 成本更低，但要理解“头共享 / 数据共享 / 生命周期”

## 3. stage02 为什么默认 `loop_mode=copy`

因为这个阶段更偏教学，`copy` 更容易让人先理解：
- TX 包被看见了
- 驱动又造出了一份 RX 包
- RX 包重新走收包路径

`clone` 作为进阶模式保留，用于帮助理解共享数据区和引用计数。

## 4. `ndo_start_xmit()` 里都做了什么

1. 更新 TX 统计
2. 根据 `loop_mode` 创建一份 RX skb
3. 清理/修正 RX 注入所需元数据
4. 调用 `eth_type_trans()` 为 RX 路径设置 protocol
5. 调用 `netif_rx()` 把包送回协议栈
6. 更新 RX/注入统计
7. 消费原始 TX skb

## 5. 为什么这里用 `netif_rx()`

因为 stage02 还没有 NAPI，也没有驱动侧 poll。

`netif_rx()` 很适合作为这一阶段的教学入口：
- 明确表示“把 skb 重新交给协议栈接收路径”
- 代码直观
- 方便后面 stage03 再对照 NAPI 的收包方式

## 6. stage02 最该讲清楚的生命周期

```text
userspace sendto()
    -> 原始 TX skb 进入驱动
    -> 驱动为 RX 注入创建 rx_skb(copy/clone)
    -> rx_skb 交给 netif_rx()
    -> 原始 TX skb 被 dev_consume_skb_any() 消费
    -> 协议栈后续继续处理 rx_skb
```

这一段就是 stage02 的核心学习闭环。
