# stage03_napi_poll 深度指南 - NAPI / poll / 中断抑制

## 一、stage03 是什么

stage03 是 netdev 主线的第三步，定位是**NAPI 框架与 poll 批量处理**。

**核心目标**：
1. 理解"每包一中断"的问题 → 中断风暴
2. 掌握 NAPI 的核心思想：中断只通知，poll 批量处理
3. 理解 pending queue、budget、complete 的语义
4. 理解 direct 模式 vs napi 模式的本质区别
5. 理解 `netif_rx()` vs `netif_receive_skb()` 的区别

stage03 不引入真实 hardware ring / DMA，因为那是 stage04 的内容。

---

## 二、为什么需要 NAPI

### 2.1 "每包一中断"的问题

```
每收到一帧 → 触发 IRQ → handler 做 RX 处理 → 返回
```

高频流量下：
- 每秒 1Gbps → 每秒约 150 万帧
- 每帧一个 IRQ → CPU 被 IRQ 打满
- 大量 CPU 时间花在 IRQ 处理上，而不是协议栈处理

### 2.2 NAPI 的核心思想

> **"中断只负责通知，真正处理交给 poll 批量完成"**

```
无包时：IRQ 被启用，CPU 空闲
    ↓
收到包 → 触发 IRQ（第一次）→ mask IRQ → schedule poll
    ↓
poll 按 budget 批量处理队列中的包
    ↓
队列空 → complete poll → unmask IRQ → 等待下一波包
```

**关键效果**：
- IRQ 次数大幅减少（从"每包一 IRQ"变成"每波包一 IRQ"）
- 批量处理减少 per-packet 开销
- CPU 时间更多花在协议栈处理上

---

## 三、netdev 学习路径中的位置

### 3.1 netdev 整体架构

```
netdev/
├── stage00_bootstrap/        ← 环境验证 + 路径固化
├── stage01_netdev_skeleton/  → 最小 net_device 骨架（TX 能触发）
├── stage02_skb_path/         → skb 生命周期 + 软件环回
├── stage03_napi_poll/        → 今天：NAPI / poll / 中断抑制 ★
├── stage04_ring_dma/         → ring / DMA / RX replenishment
├── stage05_virtio_param/     → virtio-net 对照 + 平台参数化
└── stage06_arm64_migration/   → ARM64 迁移与跨平台收口
```

### 3.2 stage02 → stage03 的演进

```
stage02（每包立即处理）：
  用户态 sendto()
      ↓
  ndo_start_xmit()
      ↓
  stage02_build_rx_skb()
      ↓
  netif_rx(rx_skb) ← 直接注入 RX 路径
      ↓
  协议栈 RX 处理
      ↓
  用户态 recvfrom()

stage03（NAPI 批处理）：
  用户态 sendto()
      ↓
  ndo_start_xmit()
      ↓
  stage03_build_rx_skb()
      ↓
  skb_queue_tail(pending_rxq) ← 先入队
      ↓
  stage03_raise_irq() ← 触发"中断"
      ↓
  napi_schedule()
      ↓
  stage03_napi_poll(budget) ← 批量 drain
      ↓
  netif_receive_skb() ← 在 poll 上下文注入
      ↓
  napi_complete_done() ← 完成，解除中断抑制
      ↓
  协议栈 RX 处理
      ↓
  用户态 recvfrom()
```

**stage02 是"看见"，stage03 是"批处理"。**

---

## 四、stage03 的教学型抽象

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

---

## 五、核心数据结构解析

### 5.1 pending_rxq

```c
struct sk_buff_head pending_rxq;
skb_queue_head_init(&priv->pending_rxq);
```

**作用**：FIFO 队列，存放等待 poll 处理的 skb

**为什么是教学替身**：
- 真实硬件用 RX ring（descriptor 数组）
- stage03 用 sk_buff_head（链表）
- 两者都是"生产者（TX 路径）入队，消费者（poll）出队"

**操作**：
```c
skb_queue_tail(&pending_rxq, skb);  // 入队（TX 路径）
skb = skb_dequeue(&pending_rxq);     // 出队（poll 路径）
```

### 5.2 napi_struct

```c
struct napi_struct napi;
netif_napi_add_weight(ndev, &priv->napi, stage03_napi_poll, napi_weight);
```

**作用**：NAPI 框架的核心数据结构，关联到 net_device

**关键字段**：
- `poll` → 指向 poll 回调函数
- `weight` → budget 上限（每次 poll 最多处理多少包）

### 5.3 irq_masked

```c
bool irq_masked;
```

**作用**：模拟硬件中断的 mask/unmask 状态

**状态机**：
```
irq_masked = false（初始状态）
    ↓
stage03_raise_irq() → irq_masked = true + napi_schedule()
    ↓
... poll 处理中 ...
    ↓
napi_complete_done() → irq_masked = false
```

**为什么需要这个状态**：
- 防止在 poll 还没处理完时，又被新的 IRQ 打断
- 真实硬件也会有类似的中断抑制逻辑

---

## 六、poll 函数的语义

### 6.1 函数签名

```c
static int stage03_napi_poll(struct napi_struct *napi, int budget)
```

- `napi`：NAPI 描述符
- `budget`：本次 poll 最多处理的包数
- 返回值：实际处理的包数

### 6.2 poll 循环

```c
while (work_done < budget) {
    skb = skb_dequeue(&priv->pending_rxq);
    if (!skb)
        break;
    netif_receive_skb(skb);
    work_done++;
}
```

### 6.3 budget 语义

**不是"总共能处理多少包"的全局限额**

而是：

> **本次 poll 调用最多允许做多少个 work item**

**在 stage03 里，一个 work item 就是：从 pending_rxq 取出一帧并注入协议栈**

### 6.4 budget_exhausted

```c
budget_exhausted = (work_done >= budget) && (skb_queue_len(&priv->pending_rxq) > 0);
```

**含义**：本次 poll 用完了 budget，但队列里还有包

**效果**：
- poll 返回 `work_done == budget`
- NAPI 框架会继续安排后续 poll
- `napi_budget_exhaust_count++`

### 6.5 napi_complete_done

```c
if (!budget_exhausted) {
    if (napi_complete_done(napi, work_done)) {
        priv->irq_masked = false;
        stage03_note_napi_complete(priv);
        stage03_note_irq_unmasked(priv);
    }
}
```

**含义**：队列已空（`!budget_exhausted`），poll 正式完成

**效果**：
- 清 `irq_masked` 标志
- 解除中断抑制，等待下一波包

---

## 七、netif_rx vs netif_receive_skb

这是 stage03 最关键的语义变化之一。

### 7.1 netif_rx()

```
调用路径：
  netif_rx(skb) → __netif_rx() → raise NET_RX_SOFTIRQ
  ↓
NET_RX_SOFTIRQ 在稍后执行 → ip_rcv() 等协议栈处理
```

**特点**：
- 非 NAPI 驱动使用
- 触发 softirq 延迟处理
- 可在中断上下文调用

### 7.2 netif_receive_skb()

```
调用路径：
  netif_receive_skb(skb) → ip_rcv() 等协议栈处理（直接调用）
```

**特点**：
- NAPI 驱动专用
- 在 poll 上下文直接处理，不经过 softirq
- 性能更好（减少了一次上下文切换）

### 7.3 在 stage03 中的使用

| 模式 | 注入 API | 说明 |
|------|----------|------|
| direct | `netif_rx()` | 沿用 stage02 路径，便于对照 |
| napi | `netif_receive_skb()` | 在 poll 上下文注入 |

这样可以直观对比两种 API 的行为差异。

---

## 八、direct vs napi 的完整对比

### 8.1 direct 模式

```text
ndo_start_xmit()
  → stage03_build_rx_skb()
  → netif_rx(rx_skb)          ← 直接注入
  → dev_consume_skb_any(skb)  ← 消费 TX skb
```

**特点**：
- 路径短
- 无排队
- 无 budget 语义
- 无中断抑制
- 便于理解 `skb` 生命周期

**debugfs 特征**：
```
direct_inject_count > 0
napi_inject_count = 0
pending_enqueued = 0
irq_raised = 0
napi_schedule_count = 0
```

### 8.2 napi 模式

```text
ndo_start_xmit()
  → stage03_build_rx_skb()
  → skb_queue_tail(pending_rxq)  ← 入队
  → stage03_raise_irq()
  → napi_schedule_prep()
  → __napi_schedule()
  → stage03_napi_poll(budget)    ← 批量 drain
    → skb_dequeue()
    → netif_receive_skb()
  → napi_complete_done()          ← 完成
  → dev_consume_skb_any(skb)     ← 消费 TX skb（入队后立即消费）
```

**特点**：
- 完整的 NAPI 语义
- 有排队与批处理
- 有 budget 限制
- 有中断抑制
- 更接近真实网络驱动

**debugfs 特征**：
```
direct_inject_count = 0
napi_inject_count > 0
pending_enqueued > 0
pending_drained > 0
irq_raised > 0
napi_schedule_count > 0
napi_poll_count > 0
napi_complete_count > 0
```

---

## 九、中断抑制的语义

### 9.1 真实硬件的中断抑制

```
收到包 → IRQ 触发 → handler mask IRQ → 处理 → unmask IRQ
```

**问题**：如果每次收包都开/关 IRQ，高频时 IRQ 开销仍然很大

### 9.2 NAPI 的中断抑制

```
收到包 → IRQ 触发 → mask IRQ + schedule poll → poll 处理期间 IRQ 被抑制
poll 完成 → unmask IRQ → 等待下一波包
```

**关键效果**：**poll 处理期间不响应新 IRQ**，直到 poll 完成才重新开中断

### 9.3 stage03 的模拟

```c
static void stage03_raise_irq(struct stage03_priv *priv)
{
    spin_lock_irqsave(&priv->state_lock, irq_flags);
    if (!priv->irq_masked) {      // 只在未 mask 时 schedule
        priv->irq_masked = true;   // 设置 mask 标志
        need_schedule = true;
    }
    spin_unlock_irqrestore(&priv->state_lock, irq_flags);

    if (need_schedule) {
        napi_schedule_prep(&priv->napi);
        __napi_schedule(&priv->napi);
    }
}
```

```c
static int stage03_napi_poll(...)
{
    // ... drain queue ...

    if (!budget_exhausted) {
        if (napi_complete_done(napi, work_done)) {
            spin_lock_irqsave(&priv->state_lock, irq_flags);
            priv->irq_masked = false;  // 清 mask 标志
            spin_unlock_irqrestore(&priv->state_lock, irq_flags);
        }
    }
}
```

---

## 十、为什么用 netif_receive_skb() 而不是 netif_rx

stage02 里没有 NAPI，所以用了 `netif_rx()`。

到了 stage03：
- RX 已经在 poll 上下文里
- 不需要再通过 `NET_RX_SOFTIRQ` 转一层
- 直接 `netif_receive_skb()` 更符合 NAPI 驱动语义

这也是 stage02 → stage03 最关键的语义变化之一。

---

## 十一、debugfs 统计详解

```
# 模式信息
ifname=nds3
rx_mode=napi              # 当前 RX 模式
loop_mode=copy            # skb 构造模式
napi_weight=8            # poll budget 上限
max_queue_depth=1024     # pending queue 最大深度

# 运行时状态
irq_masked=0             # 中断是否被抑制
pending_queue_len=0      # 当前队列深度

# TX 统计（进入驱动的发送请求）
tx_packets=45
tx_bytes=7531
tx_dropped=0
last_tx_len=339
last_tx_proto=0x0800

# RX 统计（交付给协议栈的包）
rx_packets=45
rx_bytes=6901
rx_dropped=0
last_rx_len=325
last_rx_proto=0x0800

# 设备开关统计
open_count=1
stop_count=0

# skb 构造统计
copy_built=45           # skb_copy() 次数
clone_built=0           # skb_clone() 次数
build_failures=0        # 构造失败次数

# 注入路径统计
direct_inject_count=0   # direct 模式注入次数
napi_inject_count=45    # napi 模式注入次数
netif_rx_success=0      # netif_rx() 成功次数
netif_rx_drop=0        # netif_rx() 丢弃次数
netif_receive_success=45 # netif_receive_skb() 成功次数
netif_receive_drop=0   # netif_receive_skb() 丢弃次数
last_inject_rc=0        # 最近一次注入返回值

# pending queue 统计
pending_enqueued=45     # 入队总次数
pending_drained=45      # 出队总次数
pending_dropped=0      # 队列满丢弃次数
pending_peak=1          # 队列最大深度
pending_last_depth=0   # 最近一次操作后的队列深度

# 中断统计
irq_raised=45          # 中断触发次数
irq_masked_count=45    # 中断屏蔽次数
irq_unmasked_count=45  # 中断恢复次数

# NAPI 统计
napi_schedule_count=45  # napi_schedule 次数
napi_poll_count=45     # poll 被调用次数
napi_complete_count=45 # poll 完成次数
napi_budget_exhaust_count=0 # budget 耗尽次数
napi_work_total=45     # 总处理包数
last_poll_budget=8     # 最近一次 poll 的 budget
last_poll_work=1       # 最近一次 poll 实际处理数
```

**关键指标含义**：

| 指标 | 含义 |
|------|------|
| `direct_inject_count` vs `napi_inject_count` | 区分两种注入路径 |
| `pending_peak` | 队列最大积压深度，>1 说明出现过积压 |
| `napi_budget_exhaust_count` | budget 耗尽次数，>0 说明出现过 poll 没清空队列的情况 |
| `irq_masked_count` vs `irq_unmasked_count` | 中断抑制次数，应该匹配 |
| `napi_schedule_count` vs `napi_complete_count` | schedule 和 complete 应该匹配 |

---

## 十二、面试要会讲的五句话

1. **"stage03 的核心目标是把 NAPI 的批处理语义讲清楚：通过 pending_rxq 代替硬件 ring，用软件触发 irq → napi_schedule → poll drain 的流程，让学习者理解中断抑制和 poll budget 的含义"**
   → 理解 stage03 的定位和教学模型

2. **"netif_rx() 和 netif_receive_skb() 的本质区别在于处理上下文：netif_rx() 触发 NET_RX_SOFTIRQ 延迟处理（非 NAPI），netif_receive_skb() 在 poll 上下文直接处理（NAPI）。stage03 通过 direct 模式和 napi 模式的对比让这个区别可视化"**
   → 理解两种 RX 注入方式的区别

3. **"NAPI 的中断抑制语义是：poll 开始时 mask 中断，poll 结束时才 unmask 中断。这样在 poll 处理期间，新到的包只会触发一次中断（唤醒 poll），而不是每包一中断。stage03 用 irq_masked 状态模拟了这个行为"**
   → 理解中断抑制的机制

4. **"poll 的 budget 限制的是'本次 poll 调用最多处理多少包'，而不是全局限额。如果 budget 用完了但队列还有包，说明这一波包太多，NAPI 框架会继续安排后续 poll 来 drain。stage03 用 napi_budget_exhaust_count 记录这种情况"**
   → 理解 budget 的真正含义

5. **"stage03 的 pending_rxq 是教学替身，不是最终设计。pending_rxq 在真实驱动里会被硬件 RX ring 取代，skb_dequeue() 会被'读 descriptor + DMA unmap + refill'取代。但 pending / budget / poll drain / complete 这些概念在真实驱动里依然原样保留"**
   → 理解 stage03 和 stage04 的衔接

---

## 十三、验收标准

### 13.1 功能验收

- [ ] `make build-userspace` 编译 send_stage03_frame 和 recv_stage03_frame
- [ ] `make build-module` 编译 netdev_stage03.ko
- [ ] `sudo make load` 加载模块，dmesg 显示 `registered ifname=nds3`
- [ ] `sudo ip link set nds3 up` 接口 UP
- [ ] `rx_mode=direct` 模式下发送帧成功
- [ ] `rx_mode=napi` 模式下发送帧成功

### 13.2 统计验收（direct 模式）

- [ ] `direct_inject_count > 0`
- [ ] `napi_inject_count = 0`
- [ ] `pending_enqueued = 0`
- [ ] `irq_raised = 0`
- [ ] `napi_poll_count = 0`

### 13.3 统计验收（napi 模式）

- [ ] `direct_inject_count = 0`
- [ ] `napi_inject_count > 0`
- [ ] `pending_enqueued > 0`
- [ ] `pending_drained > 0`
- [ ] `irq_raised > 0`
- [ ] `napi_schedule_count > 0`
- [ ] `napi_poll_count > 0`
- [ ] `napi_complete_count > 0`
- [ ] `irq_masked_count = irq_unmasked_count`

### 13.4 模式验收

- [ ] 默认 `loop_mode=copy` 时，`copy_built` 增加
- [ ] `loop_mode=clone` 时，`clone_built` 增加

---

## 附录 A：目录结构

```
stage03_napi_poll/
├── README.md
├── START_HERE.md
├── TASKS.md
├── Makefile
├── driver/
│   ├── Makefile
│   └── netdev_stage03.c       # 核心驱动
├── tools/
│   ├── send_stage03_frame.c    # 发包工具（支持 burst）
│   └── recv_stage03_frame.c    # 收包工具（支持超时）
├── scripts/
│   ├── smoke.sh               # 冒烟测试
│   ├── load_module.sh         # 加载模块
│   └── unload_module.sh       # 卸载模块
├── docs/
│   ├── 01_STAGE_GOAL_AND_BOUNDARY.md
│   ├── 02_NAPI_MOTIVATION_AND_MODEL.md
│   ├── 03_PENDING_QUEUE_AND_POLL_PATH.md
│   ├── 04_DIRECT_VS_NAPI_MODE.md
│   └── 05_TEST_AND_ACCEPTANCE.md
├── env/
│   └── stage03_napi_poll.env
└── output/
    ├── host_env_stage03.env
    └── stage03_report.md
```

## 附录 B：完整流程图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           stage03 NAPI 模式完整流程                           │
└─────────────────────────────────────────────────────────────────────────────┘

  用户态                         内核                              用户态
  sendto()              ndo_start_xmit()                   recvfrom()
    │                         │                                  ▲
    │   AF_PACKET/SOCK_RAW    │                                  │
    ▼                         │                                  │
    │                    stage03_build_rx_skb()                   │
    │                         │                                  │
    │                    skb_queue_tail(pending_rxq)              │
    │                         │                                  │
    │                    stage03_raise_irq()                      │
    │                         │                                  │
    │                    napi_schedule_prep()                     │
    │                         │                                  │
    │                    __napi_schedule() ───────────────────────┼──►
    │                         │                                  │
    │                    stage03_napi_poll(budget)                 │
    │                         │                                  │
    │                    skb_dequeue()                            │
    │                         │                                  │
    │                    netif_receive_skb()                       │
    │                         │                                  │
    │                    napi_complete_done()                     │
    │                         │                                  │
    │                    dev_consume_skb_any(tx_skb)               │
    │                         │                                  │
    └─────────────────────────┴──────────────────────────────────┘
                                协议栈 RX 路径

【关键点】
1. TX skb 在入队后立即被 dev_consume_skb_any() 消费
2. RX skb 在 poll 中才被 netif_receive_skb() 交付
3. irq_masked 在 poll 期间保持 true，阻止新 IRQ
4. poll 完成后 irq_masked 恢复 false
```

## 附录 C：与 stage04 的衔接点

```
stage03 结束时的知识状态：
  ✅ NAPI 框架语义
  ✅ pending queue / budget / poll drain
  ✅ complete / re-enable irq
  ✅ netif_receive_skb() 在 poll 上下文

stage04 需要解决的问题：
  ❓ pending_rxq 太简单 → 真实硬件 RX ring 是什么
  ❓ skb 从哪里来 → DMA mapping / buffer allocation
  ❓ poll drain 时只出队 → 怎么 refill 新的 RX buffer
  ❓ 真实网卡的 descriptor 格式是什么样的
```
