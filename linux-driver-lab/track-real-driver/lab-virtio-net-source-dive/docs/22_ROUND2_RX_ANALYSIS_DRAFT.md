# 22_ROUND2_RX_ANALYSIS_DRAFT

> 说明：这是一份 **Round2 / RX 主路径分析初稿**。  
> 目标是把 `virtio_net` 的 RX 过程，从“零散函数印象”整理成一条 **callback / poll / refill / skb build / 上送** 的主路径。

## 1. Round2 看 RX 时最容易犯的错

读 RX 比读 TX 更容易散，因为 RX 经常不是一条简单直线。

常见误区有三个：

1. 把中断 / callback / poll 混成一层
2. 只看 `virtnet_poll`，忽略它的上游和下游
3. 只看 skb 构建，忽略 refill / recycle / GRO / XDP 这些边界

所以 Round2 读 RX 时，最重要的不是“多看几个函数”，而是先建立下面这条总图：

```text
RX buffer 准备/refill
  -> callback / 事件触发
  -> napi schedule / poll
  -> 从 queue 取出已到达数据
  -> 构建 skb / 处理协议辅助能力
  -> GRO / checksum / XDP 边界
  -> 上送协议栈或进入回收路径
```

---

## 2. 为什么 `virtnet_poll` 是关键入口，但不能只盯它

你在自己项目里已经学过：

- `stage03_napi_poll` 会让人非常自然地把“收包核心”压缩成一个 poll 函数
- 但真实驱动里，poll 只是一个承接点，不是全部

在 `virtio_net` 里，`virtnet_poll` 之类入口的重要性主要体现在：

- 它把 callback / 事件触发转成可控的 poll 处理
- 它体现了 budget / 批处理 / 收包推进节奏
- 它是 RX queue 处理的重要协调点

但如果只盯这个函数，就会遗漏：

- 上游：是谁把它唤醒的
- 下游：它最终怎么推进到 skb 构建和上送
- 并行关系：它和 refill/recycle、GRO、XDP 的边界如何衔接

---

## 3. RX 也必须从 queue 视角理解

和 TX 一样，RX 的真正骨架也不是“某个收包函数”，而是 queue 生命周期。

### 3.1 RX 的关键不是“收到包”，而是“如何组织收包”
真实驱动里的 RX 通常要同时解决：

- 缓冲区提前准备
- 数据到达后的事件推进
- poll 批量处理
- skb 构建
- 回收 / 再补充缓冲区

所以 RX 本质上是：

> **围绕 RX queue、缓冲区生命周期和 poll 批处理组织起来的系统。**

### 3.2 这和自己的项目对应关系
最直接的映射有：

- `stage03_napi_poll`：poll / budget / 节奏控制
- `stage11_page_pool_rx`：RX buffer / page recycle / refill 思维
- `stage14_xdp_basics`：RX fast path 边界感
- `stage13_offload_basics`：GRO/checksum 等能力边界

---

## 4. refill 为什么很重要

教学型驱动里，最容易把 refill 看成一个“辅助动作”。  
但在真实驱动里，如果没有 refill 视角，RX 就会只剩“处理当前包”的局部观察。

更完整的理解应该是：

- RX 不是一次性消费
- 驱动必须不断准备可用接收缓冲区
- poll 消费之后，还要考虑如何补回资源
- 这和 page / buffer 生命周期直接相关

所以阅读 RX 时，不要只问：

> “这个包现在怎么被处理？”

还要问：

> “下一批包所需的接收资源是谁、在什么时候、以什么策略重新准备的？”

这也是为什么 `stage11_page_pool_rx` 会成为你和 `virtio_net` 对照时非常有价值的一环。

---

## 5. skb 构建：别把它看成 RX 的终点

在 Round2 里，skb 构建是一个重要节点，但不是 RX 的全部终点。

skb 构建后，后面还经常连着：

- checksum/GRO 等协议辅助能力
- XDP 与常规 skb path 的边界
- 统计与上送协议栈
- 某些失败/回收分支

如果把“拿到数据并变成 skb”当成终点，  
就会漏掉真实驱动中很重要的一层：

> **“这个包在被构建之后，还要以哪条语义路径继续向前走？”**

---

## 6. GRO / checksum / XDP：为什么这一层现在只定边界

这一轮不建议把所有 feature/offload 细节都压进 RX 正文里。  
更合理的方式是：

- 在 Round2 里先把这些能力当成 RX 主路径上的“边界节点”
- 到 Round3 再深入 feature / offload / ethtool / XDP 专题

所以在这一轮里，你只需要先回答：

- checksum/GRO 大致挂在 RX 的哪一层
- XDP 是在普通 skb path 之前还是之后参与
- 自己的 `stage13` / `stage14` 和这里的边界关系是什么

这就够了。

---

## 7. 一个最值得建立的 RX 路径图

建议你最终至少能稳定复述下面这张图：

```text
准备 RX buffer
  -> 数据到达 / callback
  -> napi schedule
  -> poll(budget)
  -> 从 RX queue 取数据
  -> 构建 skb
  -> 经过 GRO/checksum/XDP 等边界判断
  -> 上送协议栈
  -> refill / recycle
```

这张图的价值在于，它把 RX 从“几个局部函数”变成了：

- 事件
- 批处理
- 缓冲区生命周期
- 上送语义

组成的闭环。

---

## 8. RX 这一轮最应该沉淀的 4 个结论

### 结论 1
`virtnet_poll` 是关键入口，但它只是 RX 处理链中的一个节点。

### 结论 2
RX 的主骨架是 queue + buffer lifecycle + poll，而不是单个函数。

### 结论 3
refill / recycle 不是配角，而是 RX 稳定运行的必要组成部分。

### 结论 4
GRO / checksum / XDP 这一轮先确定边界，下一轮再深挖能力语义。

---

## 9. 推荐你真正写出来的 RX 分析表

### 9.1 路径表
| 阶段 | 解决什么问题 | 和自己哪个 stage 最像 |
|---|---|---|
| RX buffer 准备 | 保证队列上始终有可用接收资源 | `stage11` |
| callback / 事件触发 | 从设备事件切到驱动处理 | `stage10` |
| napi schedule / poll | 批量化 RX 处理入口 | `stage03` |
| 取数 + skb 构建 | 进入协议栈友好形式 | `stage11/stage13` |
| GRO/checksum/XDP 边界 | 能力分支 | `stage13/stage14` |
| refill / recycle | 形成资源闭环 | `stage11` |

### 9.2 差异表
| 主题 | 教学驱动 | `virtio_net` |
|---|---|---|
| poll | 便于讲概念 | 与 queue / callback / refill 关系更紧 |
| RX buffer 生命周期 | 容易局部化 | 必须作为整条路径来理解 |
| XDP/GRO/checksum | 可先拆开讲 | 真实路径里是边界节点 |

---

## 10. RX 第一版完成后的下一步

当你能稳定回答下面几个问题时，就可以认为 RX 第一版已经站住了：

1. 为什么不能只看 `virtnet_poll`？
2. 为什么 RX 必须从 queue + buffer lifecycle 理解？
3. refill / recycle 为什么是主路径的一部分？
4. GRO / checksum / XDP 在这一轮为什么只先定边界？
5. `stage03/stage11/stage13/stage14` 在这里分别映射到什么？

之后最自然的推进就是：

- `docs/06_QUEUE_NAPI_IRQ.md`
- `docs/07_FEATURES_ETHTOOL_XDP.md`
- `reports/round2_txrx_summary_draft.md`

也就是把 TX / RX 进一步和 queue / 事件模型 / 能力边界真正收拢起来。
