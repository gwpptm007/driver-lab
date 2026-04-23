# 21_ROUND2_TX_ANALYSIS_DRAFT

> 说明：这是一份 **Round2 / TX 主路径分析初稿**。  
> 目标不是把 `virtio_net` 的 TX helper 全部穷尽，而是先建立一条 **可复述、可画图、可和自己项目映射** 的发送路径。

## 1. Round2 看 TX 的正确目标

在真实驱动阅读里，TX 很容易陷入两种误区：

1. 只盯 `ndo_start_xmit`，把它误看成“全部发送逻辑”
2. 只盯 queue 提交细节，忽略后面的 notify / completion / reclaim

所以这一轮看 TX，最重要的不是记所有函数名，而是先回答：

- **发包从哪里进入驱动？**
- **包在什么位置进入 queue / virtqueue 体系？**
- **什么动作会触发 notify / kick？**
- **发送后的回收 / completion 在哪里发生？**
- **这条路径和自己 `stage04/stage09/stage10` 的对应关系是什么？**

---

## 2. TX 入口：`ndo_start_xmit` 只是起点，不是全貌

和大多数 Linux 网卡驱动一样，`virtio_net` 的 TX 也从 netdev 发包入口开始。

第一轮看 TX 时，建议把这个入口理解为：

- 网络栈把 skb 交给驱动的边界
- netdev 世界切入驱动内部 queue 世界的入口
- 一条更长发送路径的“起点”

也就是说，`ndo_start_xmit` 解决的是：

> **“包现在开始进入驱动了”**

但它还没有单独回答：

- 这个包最终怎么进入 virtqueue
- 什么时候真正通知设备
- 什么时候确认这个包已经可以被回收

---

## 3. 为什么 TX 必须从 queue 视角理解

你自己前面的教学驱动已经证明了一件事：

- 只看“发包函数”本身，不足以理解真实发送路径
- 真实发送路径的稳定性，往往取决于 queue 生命周期、提交、回收和事件协同

在 `virtio_net` 里，这一点更明显。

### 3.1 queue 不是“数据结构细节”，而是发送路径骨架
TX 的核心不是“把 skb 传下去”这么简单，而是：

- 选择/定位发送队列
- 把待发送数据组织成 queue 可接受的形式
- 提交给 virtqueue
- 在合适的时机触发 notify / kick
- 在 completion/reclaim 阶段把资源收回来

如果脱离 queue 去看 TX，就会把驱动理解成“一个函数调用链”；  
但真实驱动里，TX 本质上是：

> **围绕发送队列与设备通知机制组织的生命周期。**

### 3.2 这和自己的 stage 对应关系非常强
最强对应有三段：

- `stage04_ring_dma`：为什么发送路径不是“一次函数调用”，而是“提交 + 回收”的 ring 生命周期
- `stage09_multi_queue_scaling`：为什么不能只看单条发送路径，而要结合队列分布理解
- `stage10_msix_per_queue_irq`：为什么 completion / reclaim 往往和事件/通知模型一起看才完整

---

## 4. 一个最值得建立的 TX 路径图

Round2 不要求你把所有局部 helper 都展开，但至少应该建立下面这条抽象路径：

```text
网络栈发包
  -> ndo_start_xmit
  -> 选择/定位发送队列
  -> 组织待发送描述
  -> 提交到 virtqueue
  -> 必要时 notify / kick
  -> 等待/接收 completion
  -> reclaim / 回收发送资源
```

这张图最大的价值，是把 TX 从“一个入口函数”变成“完整生命周期”。

---

## 5. notify / kick 应该怎么理解

很多人在第一遍读 TX 时，会把 notify / kick 看成一个“顺手做的动作”。

更准确地说，它的意义是：

- 驱动把发送请求放进 queue 后
- 在合适的策略下通知设备/后端继续推进处理
- 它是发送路径推进的一部分，而不是无关紧要的尾巴

所以在阅读时，建议你把 notify / kick 放在这两个问题中理解：

1. **为什么不是每次都要立即通知？**
2. **通知动作和 queue 提交、批量化、性能之间是什么关系？**

即使你第一轮不把所有细节追完，也应该先有这种结构化视角。

---

## 6. completion / reclaim：TX 的后半段不能不看

如果你只看 enqueue，而不看 reclaim，  
就会把 TX 误以为只是“把 skb 丢给设备”。

真实驱动里，TX 的后半段同样重要：

- 哪些资源在发送后仍被占用
- 什么时候说明一个发送完成了
- 什么时候可以回收发送相关资源
- 统计和队列推进如何在这一阶段体现出来

所以阅读 TX 时，必须建立这种意识：

> **发送路径不是“交出去就结束”，而是“提交 + 完成 + 回收”的闭环。**

这也是为什么 `virtio_net` 比教学驱动更值得读：  
它能把“发送资源生命周期”这件事放到真实语境中看。

---

## 7. Round2 里 TX 最应该沉淀的 3 个结论

### 结论 1
TX 的本体不是 `ndo_start_xmit`，而是：

- 入口
- queue 提交
- notify
- completion / reclaim

这条完整路径。

### 结论 2
queue 是 TX 的主骨架，不是局部实现细节。

### 结论 3
和自己项目对照时，最值得重点映射的是：

- `stage04_ring_dma`
- `stage09_multi_queue_scaling`
- `stage10_msix_per_queue_irq`

---

## 8. 推荐你真正写出来的 TX 分析表

### 8.1 路径表
| 阶段 | 这一段解决什么问题 | 和自己哪个 stage 最像 |
|---|---|---|
| `ndo_start_xmit` | 从网络栈进入驱动 | `stage01/stage04` |
| queue 定位 | 包进入哪条发送通路 | `stage09` |
| 提交到 virtqueue | 进入真实发送资源组织 | `stage04` |
| notify / kick | 推进设备处理 | `stage10` |
| completion / reclaim | 发送资源回收闭环 | `stage04/stage10` |

### 8.2 差异表
| 主题 | 教学驱动 | `virtio_net` |
|---|---|---|
| 发送入口 | 强调概念清晰 | 强调真实队列与状态推进 |
| queue | 可人为简化 | 必须结合 virtqueue 和通知机制理解 |
| 回收 | 容易单独讲 | 必须纳入主路径闭环 |

---

## 9. 这一轮先不追什么

为了避免 Round2 失控，建议暂时先不把精力放到：

- 所有局部优化细节
- 所有 feature bit 对 TX 的影响
- 所有边界条件处理
- 所有 helper 的逐行解释

这些内容留给后续更细轮次或 patch/tracing 专题。

Round2 先做到：

> **能讲清 TX 路径图、知道关键节点、能和自己项目建立映射。**

这就已经很有价值了。

---

## 10. TX 分析完成后的下一步

当你已经能比较稳定地回答下面几个问题时，就可以认为 TX 第一版已经站住了：

1. `ndo_start_xmit` 为什么只是起点？
2. 为什么 TX 必须从 queue 视角理解？
3. notify / kick 在路径中的作用是什么？
4. 为什么 completion / reclaim 是发送路径的一部分？
5. `stage04/stage09/stage10` 在这里分别映射到哪层？

之后自然进入：

- `docs/22_ROUND2_RX_ANALYSIS_DRAFT.md`
- `docs/06_QUEUE_NAPI_IRQ.md`

也就是把 TX 与 RX 以及 queue/事件协同真正合起来看。
