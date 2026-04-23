# 19_ROUND1_ARCH_ANALYSIS_DRAFT

> 说明：这是一份 **Round1 架构 / probe 分析初稿**。  
> 目标不是覆盖 `virtio_net.c` 的全部细节，而是先把“设备模型层 → netdev 接入层 → queue/NAPI 组织层”的骨架建立起来。

## 1. 这一轮到底在看什么

如果直接从 `virtio_net.c` 往下追每个 helper，很容易很快看散。  
所以第一轮不追求“读完”，而追求三件事：

1. 把 `virtio_net` 放回正确的分层里理解
2. 把 `probe/remove` 看成驱动生命周期总入口
3. 建立自己的“结构体—函数—路径”三张表

这也是为什么本 Lab 的第一轮不直接从 TX/RX 细节入手，而先从：

- `virtio_driver`
- `probe/remove`
- `virtnet_info`
- queue
- napi
- `net_device`

这些核心实体入手。

---

## 2. `virtio_net` 要放在哪个框架里看

第一轮阅读时，建议把 `virtio_net` 放进下面三层框架。

### 2.1 设备模型层

这一层关心的是：

- 驱动如何以 `virtio_driver` 身份注册
- 设备与驱动如何匹配
- feature negotiation 在哪个阶段建立
- probe/remove 的生命周期边界是什么

这一层对应的是“为什么会进入这个驱动”，而不是“包是怎么收发的”。

### 2.2 netdev 接入层

这一层关心的是：

- `net_device` 何时被分配
- netdev 私有数据如何挂接
- `net_device_ops` 何时挂上
- ethtool ops / stats 能力从哪里接出去

这一层解决的是：  
**`virtio_net` 什么时候真正变成 Linux 网络栈眼里的一个网卡。**

### 2.3 数据路径组织层

这一层关心的是：

- RX/TX queue 如何组织
- `napi_struct` 如何与 queue 绑定
- callback / poll / notify 是如何接起来的

这一层还不要求你立刻追完 TX/RX 每一步，但必须先知道“后面该从哪里往下追”。

---

## 3. 第一轮最应该建立的核心结构体认知

建议先抓住下面几个结构体，而不是陷入大量局部 helper。

### 3.1 `struct virtnet_info`

这是理解 `virtio_net` 的第一锚点。  
它不是一个“普通私有结构体”而已，而是：

- 设备私有上下文
- queue / napi / 配置能力的组织中心
- `virtio` 世界和 `net_device` 世界之间的桥

换句话说，如果你只记 `net_device` 而忽略 `virtnet_info`，后面读 queue、poll、feature 时就会不断失去上下文。

**对照自己项目：**
- 它很像你在 `netdev/stage01~stage14` 里自己定义的 `priv` / softnic private context
- 但真实驱动里的它承载的职责更多，尤其是设备能力、队列组织、状态管理

---

### 3.2 RX/TX queue 结构

第二个锚点不是单个函数，而是 queue 组织。

你第一轮不一定要立刻搞清每个 virtqueue helper 做了什么，但至少要搞清：

- RX queue 和 TX queue 是分开的
- 它们不是简单数组，而是围绕 virtqueue / callback / napi 组织
- 后面的 TX/RX 主路径都必须回到 queue 视角去理解

**对照自己项目：**
- 对应 `stage04_ring_dma`
- 对应 `stage09_multi_queue_scaling`
- 对应 `stage10_msix_per_queue_irq`

教学驱动里，queue 往往是“为了讲概念而拆开”；  
真实驱动里，queue 是“围绕真实资源和事件流组织的”。

---

### 3.3 `napi_struct`

第一轮最容易误判的一点，是把 NAPI 只看作一个“收包函数入口”。

更准确地说：

- NAPI 是 queue / callback / poll / budget 协作的一部分
- 它不是孤立存在的
- 它必须和 RX queue 一起看

所以第一轮时，你应该先建立这种认识：

> `virtnet_poll` 之类的 poll 入口，只是 queue 驱动型 RX 处理链条中的一个节点，而不是全部。

**对照自己项目：**
- 最直接对应 `stage03_napi_poll`
- 但真实驱动里会比你的教学实现更强调：
  - queue 关联
  - 事件来源
  - callback / interrupt / poll 的衔接
  - 预算与回收的协同

---

### 3.4 `net_device`

`net_device` 在这里依然是网络栈标准入口，但在第一轮阅读里，它更像一个“外部接口壳”。

你要先建立这种分工感：

- `net_device`：给网络栈的标准接口
- `virtnet_info`：驱动内部的真实上下文中心
- queue / virtqueue / napi：真正的数据路径组织骨架

只把注意力放在 `net_device_ops` 上，会误以为这个驱动的逻辑中心在接口回调；  
但实际上，驱动的真实复杂度更多体现在 queue 和状态组织里。

---

## 4. 怎么理解 `probe()`：不要把它当成“注册网卡的函数”

第一轮最关键的阅读误区之一，是把 `probe()` 理解得太窄。

更准确地说，`probe()` 在 `virtio_net` 里承担了至少四类职责：

### 4.1 建立驱动与设备的契约
也就是：

- 确认设备能力
- 协商 feature
- 识别驱动可用的能力边界

这一步非常重要，因为它直接决定后面：

- 哪些 offload 能启用
- 哪些队列能力可用
- 哪些控制面接口有意义

---

### 4.2 建立 netdev 视图
也就是把 virtio 世界里的设备，转成 Linux 网络栈能识别和操作的网卡对象。

这一步你要重点看：

- netdev 分配
- 私有结构体挂接
- netdev ops / ethtool ops 初始化
- 最终 register

---

### 4.3 建立 queue / napi 组织
这一步才是后面 TX/RX 可读的前提。

如果不先理解 queue/napi 在 probe 阶段如何建起来，  
后面直接去看 `start_xmit` 或 `poll`，很容易失去上下文。

---

### 4.4 建立清理的镜像关系
第一轮不要只看 `probe()`，还要顺手看 `remove()`，目的不是马上吃透每一行，而是建立：

> 初始化做了哪些事，清理就必须回收哪些资源。

这会帮助你形成真实驱动阅读时很重要的一种意识：

- 分配/注册
- 初始化/使能
- 注销/释放

它们通常是成对出现的。

---

## 5. 第一轮建议你真的写出来的 3 张表

Round1 不建议只在脑子里“感觉懂了”。  
建议至少把下面 3 张表写出来。

### 5.1 结构体表
| 结构体 | 角色 | 与谁关联 | 这轮关注什么 |
|---|---|---|---|
| `virtnet_info` | 驱动私有上下文中心 | `net_device` / queue / feature | 它如何成为驱动主骨架 |
| RX/TX queue | 数据路径组织单元 | virtqueue / napi / callback | RX/TX 为什么要从 queue 视角理解 |
| `napi_struct` | poll 协作入口 | RX queue / callback | 为什么 NAPI 必须和 queue 一起看 |
| `net_device` | 对网络栈的标准外部接口 | ops / stats / ethtool | 它在本驱动里更像“对外壳” |

### 5.2 生命周期表
| 阶段 | 关注点 |
|---|---|
| driver register | 设备匹配与驱动入口 |
| probe | feature / netdev / queue / napi 初始化 |
| register_netdev | 正式进入网络栈 |
| runtime | TX / RX / poll / callback |
| remove | 注销与资源释放 |

### 5.3 自己 stage 映射表（第一轮版）
| 自己的 stage | 第一轮在 `virtio_net` 里主要看哪里 |
|---|---|
| `stage01_netdev_skeleton` | netdev alloc / init / register |
| `stage03_napi_poll` | poll / budget / callback 协作 |
| `stage09_multi_queue_scaling` | queue pair 组织 |
| `stage10_msix_per_queue_irq` | queue 与事件/中断模型关系 |
| `stage11_page_pool_rx` | RX queue 资源与回收思路 |
| `stage12~14` | 本轮只定位入口，不深挖 |

---

## 6. 第一轮真正应该得到的结论

### 结论 1
`virtio_net` 不是“多了一些回调的 net_device 驱动”，而是：

> 一个同时站在 virtio 设备模型和 netdev 框架之间的真实网络驱动前端。

### 结论 2
真正的阅读骨架不是“先找一个大函数”，而是：

- 结构体骨架
- probe/remove 生命周期
- queue/napi 组织关系

### 结论 3
第一轮不需要把 TX/RX 读到最细，但必须先知道：

- TX 后面要从 queue / start_xmit / notify / reclaim 去追
- RX 后面要从 callback / poll / refill / skb build 去追

### 结论 4
这一轮最重要的，不是“读懂所有代码”，而是：

> 建立一个不会在第二轮、第三轮崩掉的阅读框架。

---

## 7. 第一轮完成后的下一步

如果这一轮已经完成，下一步就应该自然进入：

- `docs/04_TX_PATH.md`
- `docs/05_RX_PATH.md`
- `docs/06_QUEUE_NAPI_IRQ.md`

也就是从“骨架认知”推进到“主路径认知”。

建议你在进入 Round2 前，至少确认自己已经能回答下面这几个问题：

1. `virtnet_info` 为什么是第一锚点？
2. 为什么 probe 不只是“注册网卡”？
3. 为什么 NAPI 不能脱离 queue 看？
4. 为什么要先建立分层，再读 TX/RX？

如果这四个问题还回答不稳，就不要急着往后追大量 TX/RX helper。
