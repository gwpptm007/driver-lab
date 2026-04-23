# 26_SHARE_DECK_SCRIPT

> 这是一份“组内分享 / 面试表达”压缩稿。  
> 建议控制在 8~12 分钟口述，或者转成一套简洁 PPT。

## 开场：为什么 stage14 之后不再继续 stage15

在 `netdev/stage00~stage14` 里，我已经完成了一个教学型 soft NIC 主线：

- net_device
- skb
- NAPI
- ring / multi-queue
- per-queue IRQ
- page_pool
- ethtool
- offload
- XDP entry

如果后面继续叫 `stage15/stage16`，就会把：

- 课程式实现推进
- 真实驱动专题研究

混在一起。

所以 stage14 之后，我改成：
- `track-real-driver`
- `lab-virtio-net-source-dive`

---

## 第一部分：为什么第一站选 virtio_net

因为它和我前面的阶段连续性最强。

它能让我把自己已经写过的机制，重新放进真实 Linux 驱动里看：

- `stage03` 对应 NAPI / poll
- `stage09/stage10` 对应 queue / 事件推进
- `stage11` 对应 RX resource lifecycle
- `stage12~stage14` 对应 control plane / offload / XDP

所以它非常适合作为：
**教学驱动 -> 真实驱动** 的第一跳。

---

## 第二部分：我是怎么读 virtio_net 的

我把阅读拆成 3 轮：

### Round1：架构 / probe
先不追所有 helper，只先看：
- `virtio_driver`
- probe/remove
- `virtnet_info`
- queue
- napi
- `net_device`

目标是建立分层骨架。

### Round2：TX / RX 主路径
把：
- TX：入口 + queue + notify + completion/reclaim
- RX：buffer + callback + poll + skb + refill/recycle

各自收成路径图。

### Round3：两条更高层的线
- queue / NAPI / IRQ：事件推进模型
- feature / offload / ethtool / XDP：能力边界模型

---

## 第三部分：我最后得到的认识升级

### 1. queue 不是简单数据结构
它是：
- 数据路径骨架
- 事件推进承接点
- 资源生命周期协调中心

### 2. 真正的驱动理解，不只是看函数调用
而是看：
- 事件怎么推进
- 资源怎么闭环
- 能力边界怎么建立

### 3. 真实驱动比教学驱动多出来的不是“代码量”
而是：
- 更真实的约束
- 更复杂的事件流
- 更清晰的能力边界

---

## 第四部分：后面准备怎么继续

我不会立刻跳去太重的方向，而是优先做：

1. ethtool / stats 小 patch
2. trace / 观测增强
3. queue / poll 观测点实验

这样能把这次 virtio_net 的阅读，继续推进到“真实实验”和“可展示作品”。

---

## 收尾一句话

这个 Lab 的价值，不只是“我看了 virtio_net”，而是：

> 我把自己写过的 netdev 主线，推进到了对真实 Linux 驱动可解释、可映射、可继续实验的一步。
