# stage02_skb_path 深度指南 - skb 生命周期与软件环回设计

## 一、stage02 是什么

stage02 是 netdev 主线的第二步，定位是**skb 作为核心数据对象 + 软件 TX/RX 环回**。

**核心目标**：
1. 理解 `struct sk_buff` 的本质：不止是 buffer，是承载网络数据的"对象"
2. 掌握 TX / RX 在驱动层的入口和出口
3. 理解 `skb_clone()` / `skb_copy()` 的区别与适用场景
4. 理解 `netif_rx()` 把 skb 送回协议栈的含义
5. 构建完整的软件环回教学路径

stage02 不引入 NAPI、ring、DMA，因为这些是"怎么搬运包"，而 stage02 要先回答"包是什么"。

---

## 二、netdev 学习路径中的位置

### 2.1 netdev 整体架构

```
netdev/
├── stage00_bootstrap/        ← 环境验证 + 路径固化
├── stage01_netdev_skeleton/  → 最小 net_device 骨架（TX 能触发）
├── stage02_skb_path/         → 今天：skb 生命周期 + 软件环回
├── stage03_napi_poll/        → NAPI / poll / 中断抑制
├── stage04_ring_dma/         → ring / DMA / RX replenishment
├── stage05_virtio_param/     → virtio-net 对照 + 平台参数化
└── stage06_arm64_migration/   → ARM64 迁移与跨平台收口
```

### 2.2 stage01 → stage02 的演进

```
stage01:
  用户态 sendto()
      ↓
  ndo_start_xmit()  ← 看到包到了
      ↓
  dev_consume_skb_any(skb)  ← 直接消费
      → 结束

stage02:
  用户态 sendto()
      ↓
  ndo_start_xmit()
      ↓
  stage02_build_rx_skb()  ← 用 tx_skb 构造一份 rx_skb
      ↓
  netif_rx(rx_skb)  ← 送回协议栈 RX 路径
      ↓
  dev_consume_skb_any(skb)  ← 消费原始 TX skb
      → RX 路径继续处理 rx_skb
```

**stage01 是"看见"，stage02 是"环回"。**

---

## 三、为什么 stage02 先不谈 ring / DMA

这个问题在面试和深入理解时经常被问到。

```
ring / DMA 回答的是"怎么搬运包"
但在你真正理解：
  - 包是什么
  - 包进入/离开驱动的边界在哪里
  - TX / RX 分别在哪一层完成交接
之前，过早进入搬运机制，反而会让概念倒置。
```

**正确定位：先理解处理对象，再理解搬运机制。**

---

## 四、skb 到底是什么

### 4.1 skb 不只是 buffer

`struct sk_buff` 是 Linux 网络栈贯穿 TX/RX 的核心数据对象。它至少承载三类信息：

```
struct sk_buff 包含：
  1. 数据区：head / data / tail / end 指针
  2. 以太头、L3/L4 数据、payload
  3. 元数据：protocol、dev、pkt_type、len、ip_summed、skb_iif 等
```

### 4.2 skb 的内存布局

```
skb->head     ┌──────────────────────────────┐
              │          headroom            │
skb->data ────►├──────────────────────────────┤
              │          payload              │
skb->tail ────►├──────────────────────────────┤
              │          tailroom             │
skb->end      └──────────────────────────────┘
```

- `headroom`：预留空间（用于添加协议头）
- `tailroom`：预留空间（用于追加数据）
- `data` 和 `tail` 会随协议栈处理推进

---

## 五、skb_clone vs skb_copy

这是 stage02 的核心教学点之一。

### 5.1 skb_copy()

```
行为：
  1. 分配全新的 struct sk_buff
  2. 分配全新的数据区（ memcpy 原数据）
  3. 两份独立的数据，互不影响

内存成本：高（数据真正复制了一份）

适用场景：
  - 需要修改数据而不影响原始 skb
  - 教学场景：直观理解"TX 抓到一帧后，造出一份 RX 包"
```

### 5.2 skb_clone()

```
行为：
  1. 分配全新的 struct sk_buff（头）
  2. 共享原始数据区（引用计数 +1）
  3. 头独立，数据共享

内存成本：低（只分配头部）

适用场景：
  - 只需要不同 skb 头（元数据）
  - 不需要修改数据
  - 性能敏感路径
```

### 5.3 图示对比

```
原始 tx_skb:
  skb_shinfo(tx_skb)->dataref = 1
  ┌─────────────────────────────┐
  │     shared data (ref=1)    │
  └─────────────────────────────┘

skb_copy(tx_skb) 后:
  ┌─────────────────────────────┐
  │     tx_skb data (ref=1)     │
  └─────────────────────────────┘
  ┌─────────────────────────────┐
  │     rx_skb data (ref=1)     │  ← 独立复制
  └─────────────────────────────┘

skb_clone(tx_skb) 后:
  ┌─────────────────────────────┐
  │     shared data (ref=2)     │
  └─────────────────────────────┘
       ▲                ▲
       │                │
  tx_skb->data    rx_skb->data
```

---

## 六、软件环回的实现

### 6.1 stage02_build_rx_skb() 详解

```c
static int stage02_build_rx_skb(struct sk_buff *tx_skb,
                                struct stage02_priv *priv)
{
    struct sk_buff *rx_skb;
    bool built_by_clone = stage02_loop_mode_is_clone();
    __be16 rx_proto;
    int rc;

    // 1. 根据 loop_mode 选择 clone 或 copy
    if (built_by_clone)
        rx_skb = skb_clone(tx_skb, GFP_ATOMIC);
    else
        rx_skb = skb_copy(tx_skb, GFP_ATOMIC);

    if (!rx_skb)
        return -ENOMEM;

    // 2. 把 skb 从"TX 上下文"转成"RX 上下文"
    skb_orphan(rx_skb);          // 断开原有 socket 关联
    rx_skb->dev = priv->ndev;     // 设置目标设备
    rx_skb->pkt_type = PACKET_HOST;  // 设为接收类型
    rx_skb->skb_iif = 0;          // 设置接口索引
    rx_skb->ip_summed = CHECKSUM_UNNECESSARY;

    // 3. eth_type_trans() 解析以太头，设置 protocol 和 pkt_type
    skb_reset_mac_header(rx_skb);
    rx_skb->protocol = eth_type_trans(rx_skb, priv->ndev);
    rx_proto = rx_skb->protocol;

    // 4. 送回协议栈 RX 路径
    rc = netif_rx(rx_skb);

    // 5. 记录统计
    stage02_note_rx_inject(priv, tx_skb->len, rx_proto,
                           built_by_clone, rc);
    return rc;
}
```

### 6.2 关键元数据设置解释

| 元数据 | 值 | 为什么 |
|--------|-----|--------|
| `skb_orphan()` | - | 断开与原 socket 的关联，防止析构时出问题 |
| `rx_skb->dev = ndev` | nds2 | 让协议栈知道这个包从哪个设备进来 |
| `rx_skb->pkt_type = PACKET_HOST` | HOST | 表示这是发给本机的包 |
| `rx_skb->skb_iif = 0` | 0 | 接收接口索引，0 表示未知 |
| `rx_skb->ip_summed = CHECKSUM_UNNECESSARY` | - | 告诉协议栈 checksum 已验证 |
| `eth_type_trans()` | - | 解析以太头，确定上层协议（IPv4/IPv6/ARP）|

### 6.3 netif_rx vs netif_rx_ni

```
netif_rx():
  - 可在中断上下文调用
  - 使用 softirq（NET_RX_SOFTIRQ）延迟处理
  - 性能更好

netif_rx_ni():
  - 必须用在进程上下文
  - 同步处理包
  - 简单但延迟高

stage02 选择 netif_rx() 是因为 start_xmit 可能运行在软中断上下文。
```

---

## 七、TX / RX 交接边界

### 7.1 驱动的 TX 出口

```
ndo_start_xmit() 返回后：
  - 返回 NETDEV_TX_OK：驱动已消费 skb（或成功交付给硬件）
  - 返回 NETDEV_TX_BUSY：硬件 TX 队列满，协议栈稍后重传
```

### 7.2 驱动的 RX 入口

```
netif_rx() 调用后：
  - skb 进入协议栈 RX 路径
  - 最终被对应协议的 handler 处理（如 ip_rcv()）
  - 或被用户 socket 接收（如果匹配 AF_PACKET bind）
```

### 7.3 stage02 的完整调用链

```
用户态: sendto(AF_PACKET, ETHERTYPE=0x88B5)
    ↓
内核协议栈: __dev_queue_xmit()
    ↓
ndo_start_xmit: stage02_start_xmit()
    ↓
stage02_note_tx() ← TX 统计
    ↓
stage02_build_rx_skb()
    ├── skb_clone() 或 skb_copy()
    ├── skb_orphan()
    ├── rx_skb->dev = ndev
    ├── rx_skb->pkt_type = PACKET_HOST
    └── eth_type_trans()
    ↓
netif_rx(rx_skb) ← 注入 RX 路径
    ↓
协议栈 RX 路径处理
    ↓
用户态: recvfrom(AF_PACKET) ← 收到环回帧
    ↓
dev_consume_skb_any(skb) ← 消费原始 TX skb
```

---

## 八、为什么用 netif_rx() 而不是 netif_receive_skb

```
netif_rx():
  - 旧接口，使用 NET_RX_SOFTIRQ
  - 适合非 NAPI 驱动
  - stage02 的教学选择

netif_receive_skb():
  - NAPI 驱动使用
  - 直接在 poll 上下文处理
  - stage03 会详细讲
```

stage02 用 `netif_rx()` 是因为它够简单，适合作为"把包重新交给协议栈"的教学入口。

---

## 九、loop_mode=copy vs clone 的教学意义

### 9.1 copy 模式（默认）

```
教学意义：直观
  - "我发送了一帧，驱动又造了一帧新的收到我"
  - 两份数据完全独立
  - 适合第一遍学习 skb 环回
```

### 9.2 clone 模式

```
教学意义：理解共享与引用计数
  - "我发送了一帧，驱动基于我的数据造了一个新的 skb 头"
  - 数据区是共享的（引用计数=2）
  - 适合进阶理解 skb 生命周期
  - 更接近真实驱动的高性能场景
```

---

## 十、debugfs stats 详解

```
ifname=nds2
loop_mode=copy                    # 当前环回模式
open_count=1                      # ndo_open 调用次数
stop_count=0                      # ndo_stop 调用次数
tx_packets=17                     # 发送帧数
tx_bytes=2742                     # 发送字节数
tx_dropped=0                      # TX 丢包数
rx_packets=17                     # 环回 RX 帧数
rx_bytes=2742                     # 环回 RX 字节数
rx_dropped=0                      # RX 丢包数
loop_injected=17                   # 注入协议栈次数
copy_built=17                     # copy 模式构造次数
clone_built=0                      # clone 模式构造次数
netif_rx_success=17               # netif_rx 成功次数
netif_rx_drop=0                   # netif_rx 返回 DROP 次数
last_netif_rx_rc=1                # 最近一次 netif_rx 返回码
last_tx_len=35                    # 最近一次 TX 帧长
last_tx_proto=0x88b5              # 最近一次 TX ETHERTYPE
last_rx_len=35                    # 最近一次 RX 帧长
last_rx_proto=0x88b5              # 最近一次 RX ETHERTYPE
```

**关键指标含义：**
- `netif_rx_success vs netif_rx_drop`：区分"成功注入"和"被协议栈丢弃"
- `copy_built vs clone_built`：验证 loop_mode 参数是否生效
- `last_netif_rx_rc`：NET_RX_SUCCESS(0) / NET_RX_DROP(1) / NET_RX_CONE_LOW(2)

---

## 十一、面试要会讲的五句话

1. **"stage02 的核心目标是把 skb 作为 Linux 网络栈的核心数据对象真正吃透：通过 skb_clone/skb_copy 在 TX 路径抓取一帧，然后用 netif_rx() 重新注入 RX 路径，形成最小的软件环回教学路径"**
   → 理解 stage02 的定位和教学模型

2. **"skb_clone 和 skb_copy 的本质区别在于数据区是否共享：clone 只分配新 skb 头共享数据区（引用计数+1），copy 分配独立数据区（两份独立）；stage02 通过 loop_mode 参数让学习者直观感受两者的区别"**
   → 理解 skb 两种复制方式的区别与适用场景

3. **"ndo_start_xmit 返回 NETDEV_TX_OK 表示驱动已消费 skb（或者成功交付给硬件），但并不代表对端已收到；真实网卡返回 BUSY 表示硬件 TX 队列满，协议栈稍后会重传"**
   → 理解 TX 路径的完成语义

4. **"eth_type_trans() 的核心作用是解析以太头确定上层协议（IPv4/IPv6/ARP），同时设置 skb->protocol 和 skb->pkt_type；这是驱动向协议栈交接的关键一步"**
   → 理解 RX 入口的关键元数据设置

5. **"netif_rx() 和 netif_receive_skb() 的区别在于 softirq vs poll 上下文：netif_rx 使用 NET_RX_SOFTIRQ 延迟处理（适合非 NAPI），netif_receive_skb 在 NAPI poll 上下文直接处理（stage03 会详细讲）"**
   → 理解两种 RX 注入方式的区别

---

## 十二、验收标准

### 12.1 功能验收

- [ ] `make build-userspace` 编译 send_stage02_frame 和 recv_stage02_frame
- [ ] `make build-module` 编译 netdev_stage02.ko
- [ ] `sudo make load` 加载模块，dmesg 显示 `loaded, ifname=nds2`
- [ ] `sudo ./tools/send_stage02_frame nds2 hello` 发送帧
- [ ] `sudo ./tools/recv_stage02_frame nds2` 能收到环回的帧
- [ ] `cat /sys/kernel/debug/netdev_stage02/stats` 显示完整统计

### 12.2 统计验收

- [ ] TX 帧被记录（tx_packets > 0）
- [ ] RX 帧被环回（rx_packets > 0）
- [ ] `netif_rx_success` 与 `loop_injected` 匹配
- [ ] `copy_built + clone_built = loop_injected`

### 12.3 模式验收

- [ ] 默认 `loop_mode=copy` 时，`copy_built` 增加
- [ ] `loop_mode=clone` 时，`clone_built` 增加

---

## 附录 A：目录结构

```
stage02_skb_path/
├── README.md
├── START_HERE.md
├── TASKS.md
├── Makefile
├── driver/
│   ├── Makefile
│   └── netdev_stage02.c       # 核心驱动
├── tools/
│   ├── send_stage02_frame.c   # 发包工具
│   └── recv_stage02_frame.c   # 收包工具
├── scripts/
│   ├── smoke.sh               # 冒烟测试
│   ├── load_module.sh         # 加载模块
│   └── unload_module.sh       # 卸载模块
├── docs/
│   ├── 01_STAGE_GOAL_AND_BOUNDARY.md
│   ├── 02_SKB_LIFECYCLE_AND_DESIGN.md
│   ├── 03_SOFTWARE_LOOPBACK_PATH.md
│   ├── 04_TEST_AND_ACCEPTANCE.md
│   └── 05_DEEP_LEARNING.md    # 本文件
├── env/
│   └── stage02_skb_path.env
└── output/
    ├── host_env_stage02.env
    └── stage02_report.md
```

## 附录 B：完整流程图

```
┌──────────────────────────────────────────────────────────────┐
│                     stage02 软件环回流程                       │
└──────────────────────────────────────────────────────────────┘

  用户态                        内核                         用户态
  sendto()               ndo_start_xmit()              recvfrom()
    │                         │                            ▲
    │   AF_PACKET/SOCK_RAW    │                            │
    ▼                         │                            │
    │                    stage02_build_rx_skb()            │
    │                         │                            │
    │                    skb_clone/skb_copy                │
    │                         │                            │
    │                    skb_orphan()                      │
    │                         │                            │
    │                    eth_type_trans()                   │
    │                         │                            │
    │                    netif_rx(rx_skb) ───────────────────┼───►
    │                         │                            │
    │                    dev_consume_skb_any(tx_skb)        │
    │                         │                            │
    └─────────────────────────┴────────────────────────────┘
                          协议栈 RX 路径
```

## 附录 C：与 stage03 的衔接点

```
stage02 结束时的知识状态：
  ✅ skb 是什么（数据 + 元数据）
  ✅ TX/RX 在驱动的入口出口
  ✅ netif_rx() 注入 RX 路径
  ✅ skb_clone/skb_copy 区别

stage03 需要解决的问题：
  ❓ 每包一个硬中断 → 中断太多怎么办
  ❓ netif_rx() 在软中断处理 → 大量小包时软中断开销大
  ❓ 能不能批处理RX包
  ❓ poll/budget 机制是什么
```
