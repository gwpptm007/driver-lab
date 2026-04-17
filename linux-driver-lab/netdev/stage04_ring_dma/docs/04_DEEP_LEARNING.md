# stage04 深度指南 — ring / DMA / RX replenishment

## 一、stage04 在整个学习路径中的位置

stage04 是 W5（DMA + performance）的第四天，承接 stage03 的 NAPI 概念，引入 **descriptor ring + streaming DMA + RX buffer replenishment**。

```
W5: DMA + performance (day29-35)
├── day29: DMA 基础 (dma_alloc_coherent / dma_map_page)
├── day30: mmap 零拷贝
├── day31: benchmarking 吞吐量 / 延迟
├── day32: perf / ftrace 性能分析
├── day33: stage04 引入 ring + DMA              ← 今天
├── day34: 稳定性 + 回归测试
└── day35: W5 收口
```

stage04 的本质：**不模拟完整网卡，而是把四个核心概念落地成可观察的教学型实现**

1. descriptor（TX/RX 队列槽位）
2. ownership（CPU / device 之间的 buffer 所有权）
3. streaming DMA（dma_map_single / dma_unmap_single）
4. RX replenishment（处理完一个 slot 后重新补 fresh buffer）

---

## 二、整体架构

### 2.1 数据流总览

```
userspace sender (send_stage04_frame)
       │
       ▼
┌─────────────────────────────────────────────────────────────┐
│  ndo_start_xmit()              【TX 路径】                   │
│                                                             │
│  1. skb_is_nonlinear → skb_linearize                        │
│  2. dma_map_single(skb->data, DMA_TO_DEVICE)               │
│  3. stage04_find_posted_rx_slot() ← 找 POSTED+DEV 的 RX 槽  │
│  4. memcpy(skb->data → RX buffer)  ← 模拟 device DMA copy   │
│  5. RX desc: DONE + CPU owner                               │
│  6. stage04_raise_irq() → napi_schedule()                  │
│  7. dma_unmap_single(TX)                                    │
│  8. dev_consume_skb_any()                                   │
└─────────────────────────────────────────────────────────────┘
       ▲
       │  napi_schedule()
       │
┌─────────────────────────────────────────────────────────────┐
│  napi_poll()                   【RX 路径】                   │
│                                                             │
│  1. 找 DONE + CPU owner 的 RX desc                          │
│  2. dma_unmap_single(RX buffer, DMA_FROM_DEVICE)          │
│  3. skb_put(skb, len)                                      │
│  4. eth_type_trans() → 解析 ethertype，设置 skb->protocol  │
│  5. netif_receive_skb(skb) → 送上协议栈                     │
│  6. stage04_refill_rx_slot() → 立刻补 fresh buffer         │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 descriptor 状态机

**TX descriptor**：
```
EMPTY ──(xmit 开始)──> BUSY ──(完成)──> EMPTY
```

**RX descriptor**：
```
EMPTY ──(refill)──> POSTED + DEV owner
   ▲                        │
   │                        ▼ (device 写完)
   │                  DONE + CPU owner
   │                        │
   └──────(poll drain)───────┘
```

### 2.3 核心数据结构

```c
struct stage04_tx_desc {
    dma_addr_t dma_addr;   // skb->data 的 DMA 地址（TX 用）
    u32 data_len;          // 数据长度
    u8 owner;              // OWNER_CPU / OWNER_DEV
    u8 state;              // EMPTY / BUSY
};

struct stage04_rx_desc {
    struct sk_buff *skb;   // 预分配的 skb（RX 用）
    dma_addr_t dma_addr;  // skb->data 的 DMA 地址
    u32 buf_len;          // buffer 总大小
    u32 data_len;         // 实际数据长度
    u8 owner;              // OWNER_CPU / OWNER_DEV
    u8 state;              // EMPTY / POSTED / DONE
};
```

---

## 三、TX 路径详解

### 3.1 完整流程

```c
// netdev_stage04.c 第 624 行
static netdev_tx_t stage04_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
    // 1. 非线性 skb 处理
    if (unlikely(skb_is_nonlinear(skb))) {
        if (skb_linearize(skb)) { /* drop */ }
    }

    // 2. 建立 DMA 映射（TX 方向）
    tx_dma = dma_map_single(&ndev->dev, skb->data, skb->len, DMA_TO_DEVICE);

    // 3. 取 TX desc，检查 ring 是否满
    txd = &priv->tx_ring[tx_prod];
    if (txd->state == STAGE04_DESC_BUSY) { /* drop */ }

    // 4. 找一个 POSTED + DEV owner 的 RX slot
    //    （TX 路径复用 RX ring 作为"device 内存"）
    if (stage04_find_posted_rx_slot(priv, &rx_idx)) {
        // 没有可用的 RX slot → 丢包
        stage04_count_rx_no_desc(priv);
        return NETDEV_TX_OK;
    }

    // 5. 模拟 device DMA copy：memcpy(TX skb → RX buffer)
    rxd = &priv->rx_ring[rx_idx];
    copy_len = min_t(u32, skb->len, rxd->buf_len);
    memcpy(rxd->skb->data, skb->data, copy_len);

    // 6. 标记 RX desc DONE + CPU owner
    rxd->data_len = copy_len;
    rxd->owner = STAGE04_OWNER_CPU;
    rxd->state = STAGE04_DESC_DONE;

    // 7. 触发 NAPI
    stage04_raise_irq(priv);

    // 8. 释放 TX DMA 映射
    dma_unmap_single(&ndev->dev, tx_dma, skb->len, DMA_TO_DEVICE);

    return NETDEV_TX_OK;
}
```

### 3.2 TX 路径的"教学型 device"模拟

stage04 没有真实硬件，所以用 **RX ring 的 posted buffer** 作为"device 内存"：

- TX 路径往 RX buffer 里 memcpy 数据 → 模拟"设备从 DMA 内存读走数据"
- RX desc 标记 DONE + CPU owner → 模拟"设备写完，通知 CPU"

### 3.3 关键调试打印

```c
// 第 696 行
pr_info("[stage04] TX RXIDX=%u SKBLEN=%u CPYLEN=%u ETH=%04x\n",
    rx_idx, skb->len, copy_len,
    ntohs(*(__be16 *)(skb->data + ETH_HLEN)));
// ETH=%04x 打印 skb->data 偏移 ETH_HLEN=14 处的前 2 字节（ethertype）
// 发送 ethertype=0x88B7 时，应显示 88b7
```

### 3.4 TX 失败场景

| 失败场景 | 计数 | 结果 |
|---------|------|------|
| skb 非线性且 linearize 失败 | `tx_linearize_count++` | 丢包 |
| DMA map 失败 | `tx_dma_map_fail++` | 丢包 |
| TX ring 满 | `tx_busy_drop++` | 丢包 |
| 没有 posted RX slot | `rx_no_desc_drop++` | 丢包 |

---

## 四、RX 路径详解

### 4.1 完整流程

```c
// 第 543 行
static int stage04_poll(struct napi_struct *napi, int budget)
{
    while (work_done < budget) {
        // 1. 取 RX poll 位置 的 desc
        idx = priv->rx_poll_pos;
        rxd = &priv->rx_ring[idx];

        // 2. 必须是 DONE + CPU owner 才处理
        if (rxd->state != STAGE04_DESC_DONE ||
            rxd->owner != STAGE04_OWNER_CPU || !rxd->skb) {
            break;
        }

        // 3. 取出信息，清空 desc
        skb = rxd->skb;
        dma_addr = rxd->dma_addr;
        len = rxd->data_len;
        rxd->skb = NULL;
        rxd->owner = STAGE04_OWNER_CPU;
        rxd->state = STAGE04_DESC_EMPTY;

        // 4. DMA 解映射（RX 方向）
        dma_unmap_single(&ndev->dev, dma_addr, buf_len, DMA_FROM_DEVICE);

        // 5. 设置 skb 长度
        skb_put(skb, len);

        // 6. eth_type_trans 解析 Ethernet header
        proto = eth_type_trans(skb, priv->ndev);

        // 7. 送上协议栈
        rc = netif_receive_skb(skb);

        // 8. 立刻补 fresh buffer 回该 slot
        stage04_refill_rx_slot(priv, idx);

        work_done++;
    }

    // 9. 全部 drain 完，complete NAPI
    if (!exhausted)
        napi_complete_done(napi, work_done);

    return work_done;
}
```

### 4.2 eth_type_trans 的作用

`eth_type_trans(skb, dev)` 做了三件事：

1. **`skb_pull(skb, ETH_HLEN)`** — 剥除 Ethernet header（14 字节），`skb->data` 指向 payload
2. **`skb->protocol = ethertype`** — 从 header 解析并设置协议类型
3. **`skb->dev = dev`** — 设置所属 net_device

```c
proto = eth_type_trans(skb, priv->ndev);
// proto 的值来自 skb->data 前 2 字节（ethertype）
// 这就是为什么调试时要打印 PROTO=%04x
```

### 4.3 RX replenishment

每个 poll 循环结束后，立即调用 `stage04_refill_rx_slot()` 把同一个 slot 重新填满：

```c
// 第 445 行
static int stage04_refill_rx_slot(struct stage04_priv *priv, u16 idx)
{
    // 1. 分配新 skb
    skb = netdev_alloc_skb_ip_align(priv->ndev, priv->rx_buf_size);

    // 2. DMA 映射（DMA_FROM_DEVICE：device 可写）
    dma_addr = dma_map_single(&priv->ndev->dev, skb->data,
                              priv->rx_buf_size, DMA_FROM_DEVICE);

    // 3. 标记 POSTED + DEV owner（device 可以写这块 buffer）
    rxd->owner = STAGE04_OWNER_DEV;
    rxd->state = STAGE04_DESC_POSTED;
}
```

**关键点**：replenishment 是**同步**在 poll 循环里做的，而不是 defer 到别处。这保证 ring 始终是满的。

### 4.4 关键调试打印

```c
// 第 588 行
pr_info("[stage04] POLL IDX=%u LEN=%u PROTO=%04x RC=%d\n",
    idx, len, ntohs(proto), rc);
// PROTO 来自 eth_type_trans 解析结果
// RC 是 netif_receive_skb 返回值：0 = NET_RX_SUCCESS
```

---

## 五、DMA 概念

### 5.1 streaming DMA vs coherent DMA

| 类型 | API | 用途 |
|------|-----|------|
| streaming DMA | `dma_map_single()` / `dma_unmap_single()` | 每包传输，CPU 可重用 buffer |
| coherent DMA | `dma_alloc_coherent()` | 长期占用，CPU 和 device 共享 |

stage04 使用 **streaming DMA**（每包映射/解映射），因为 RX buffer 处理完后会被回收复用。

### 5.2 DMA 方向

```c
// TX 路径：CPU → device
dma_map_single(dev, skb->data, len, DMA_TO_DEVICE);

// RX 路径：device → CPU
dma_map_single(dev, skb->data, buf_len, DMA_FROM_DEVICE);
```

### 5.3 dma_sync

```c
// TX memcpy 之前：确保 device 看到最新数据（TX 场景）
dma_sync_single_for_device(dev, dma_addr, buf_len, DMA_FROM_DEVICE);
memcpy(rxd->skb->data, skb->data, copy_len);

// RX memcpy 之后：确保 CPU 看到 device 写完的数据
dma_sync_single_for_cpu(dev, dma_addr, copy_len, DMA_FROM_DEVICE);
```

---

## 六、packet_type handler

### 6.1 为什么需要

`netif_receive_skb()` 收到 skb 后，根据 `skb->protocol` 查找注册的 `packet_type.func` 并调用。如果 ethertype 0x88B7 没有注册，skb 被当作"未知协议"丢弃。

### 6.2 实现

```c
// 第 515 行
static int stage04_rx_pkt_type_func(struct sk_buff *skb, struct net_device *dev,
                                     struct packet_type *ptype,
                                     struct net_device *orig_dev)
{
    return NET_RX_SUCCESS;  // 表示"已处理，不要 drop"
}

// 第 1003 行（init 中）
priv->rx_pkt_type.type = htons(0x88B7);
priv->rx_pkt_type.dev = ndev;
priv->rx_pkt_type.func = stage04_rx_pkt_type_func;
dev_add_pack(&priv->rx_pkt_type);
```

这是一个 **noop handler**：返回 SUCCESS 表示"处理成功"，实际 skb 已经在 poll 中送到了 `netif_receive_skb`。handler 的存在只是为了防止 0x88B7 被当作未知协议 drop。

---

## 七、模块生命周期

### 7.1 init 顺序

```
module_init(stage04_init)
  └─→ stage04_init()
        │
        ├─1. alloc_etherdev_mqs(..., sizeof(priv), 1, 1)
        │       → 分配 net_device + stage04_priv
        │
        ├─2. eth_hw_addr_random() / snprintf(ndev->name)
        │       → 设置随机 MAC 和接口名
        │
        ├─3. ndev->netdev_ops = &stage04_netdev_ops
        │       → 绑定 .ndo_open / .ndo_stop / .ndo_start_xmit / .ndo_get_stats64
        │
        ├─4. stage04_prepare_dma_caps(ndev)
        │       → dma_set_mask_and_coherent(DMA_BIT_MASK(64))
        │
        ├─5. spin_lock_init() / u64_stats_init()
        │       → 初始化 ring_lock 和统计同步
        │
        ├─6. STAGE04_NETIF_NAPI_ADD(ndev, &priv->napi, stage04_poll, weight)
        │       → 注册 NAPI poll 回调（Linux 6.8+ 用 netif_napi_add_weight）
        │
        ├─7. stage04_init_rings(priv)
        │       ├─ kcalloc(tx_ring) / kcalloc(rx_ring)
        │       └─ for(i=0→ring_size) stage04_refill_rx_slot(priv, i)
        │               → 预投递所有 RX buffer（POSTED + DEV owner）
        │
        ├─8. register_netdev(ndev)
        │       → netdev 注册到内核，之后 ifconfig/ip link 可见
        │
        ├─9. dev_add_pack(&priv->rx_pkt_type)
        │       → 注册 ethertype 0x88B7 handler（stage04_rx_pkt_type_func）
        │
        └─10. debugfs_create_dir() / debugfs_create_file()
                → 创建 /sys/kernel/debug/netdev_stage04/{stats,rings}
```

### 7.2 exit 顺序（关键！）

```
rmmod netdev_stage04
  └─→ stage04_exit()
        │
        ├─1. napi_disable(&priv->napi)
        │       → 禁止新的 poll 调用（poll 函数不再被 schedule）
        │
        ├─2. netif_tx_disable(stage04_dev)
        │       → 禁止新的 xmit 调用（ndo_start_xmit 返回 NETDEV_TX_BUSY）
        │
        ├─3. unregister_netdev(stage04_dev)
        │       → 内核等待所有 dev_hold 引用释放后才返回
        │       → 此时 ifconfig/ip link 不再可见
        │
        ├─4. dev_remove_pack(&priv->rx_pkt_type)
        │       → 移除 ethertype 0x88B7 handler（新包不再进来）
        │
        ├─5. stage04_debugfs_exit(priv)
        │       → debugfs_remove_recursive(priv->dbg_dir)
        │
        ├─6. netif_napi_del(&priv->napi)
        │       → 从系统移除 NAPI 结构
        │
        ├─7. stage04_cleanup_rings(priv)
        │       ├─ for(i=0→ring_size) dma_unmap_single(DMA_FROM_DEVICE)
        │       ├─ for(i=0→ring_size) dev_kfree_skb_any(skb)
        │       └─ kfree(rx_ring) / kfree(tx_ring)
        │
        └─8. free_netdev(stage04_dev)
                → 释放 net_device 和 stage04_priv
```

**错误顺序会导致**：
- `dev_remove_pack()` 太早 → 新包进来 crash
- `napi_disable()` 太晚 → poll 还在跑时 ring 被释放 → UAF
- `unregister_netdev()` 前没有 `napi_disable` → 死锁（used=-1）

---

## 七·副、完整调用链总览

### 7.3 TX 路径（userspace send → kernel → NAPI poll → userspace recv）

```
send_stage04_frame.c: main()
  ├─ socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))
  ├─ bind(sockfd, &sockaddr_ll, ...)
  │       → 绑定到 ifindex + ETH_P_ALL（接收所有 ethertype）
  └─ sendto(sockfd, frame, frame_len, 0, &sockaddr_ll, sizeof(sockaddr_ll))
          → kernel: dev_hard_start_xmit()
                  └─→ netdev_stage04: ndo_start_xmit = stage04_start_xmit()
                          │
                          ├─1. skb_linearize(skb)           [非线性 skb 处理]
                          ├─2. dma_map_single(skb->data, DMA_TO_DEVICE)
                          │       → tx_dma（TX 方向的 DMA 地址）
                          ├─3. spin_lock_irqsave(ring_lock)
                          │       → 取 tx_prod，检查 TX desc BUSY
                          ├─4. stage04_find_posted_rx_slot(priv, &rx_idx)
                          │       → 从 rx_hw_pos 向前找 POSTED + DEV owner
                          │       → 没找到 → rx_no_desc_drop++ → 丢包
                          ├─5. memcpy(rxd->skb->data, skb->data, copy_len)
                          │       → 模拟 device DMA copy（TX 写入 RX buffer）
                          │       → dma_sync_for_device / dma_sync_for_cpu
                          ├─6. rxd->owner = CPU, rxd->state = DONE
                          │       → rx_posted--, rx_done++, rx_hw_pos++
                          │       → pr_info("TX RXIDX=%u SKBLEN=%u CPYLEN=%u ETH=%04x")
                          ├─7. dma_unmap_single(tx_dma, DMA_TO_DEVICE)
                          │       → 释放 TX DMA 映射
                          ├─8. dev_consume_skb_any(skb)
                          │       → 释放 TX skb
                          ├─9. spin_unlock_irqrestore(ring_lock)
                          └─10. stage04_raise_irq(priv)
                                  → irq_masked 标志检查（防重入）
                                  → napi_schedule(&priv->napi)
                                          → NAPI softirq 触发
                                                  └─→ stage04_poll()

NAPI softirq: net_rx_action
  └─→ stage04_poll(napi, budget)
          │
          ├─1. spin_lock_irqsave(ring_lock)
          │       → 取 rx_poll_pos 位置的 desc
          │       → 检查 state==DONE && owner==CPU && skb!=NULL
          │       → 否则 break（没有更多 done desc）
          ├─2. 取出 skb/dma_addr/len，清空 desc
          │       → rx_poll_pos++, rx_done--
          ├─3. dma_unmap_single(dma_addr, DMA_FROM_DEVICE)
          │       → RX DMA 解映射（device → CPU 完成）
          ├─4. skb_put(skb, len)
          │       → 设置 skb 实际数据长度
          ├─5. eth_type_trans(skb, ndev)
          │       → skb_pull(ETH_HLEN) 剥除 Ethernet header
          │       → skb->protocol = ethertype
          │       → pr_info("POLL IDX=%u LEN=%u PROTO=%04x RC=%d")
          ├─6. netif_receive_skb(skb)
          │       → 送上 Linux 协议栈
          │       → 此时 recvfrom() 收到完整 Ethernet frame
          └─7. stage04_refill_rx_slot(priv, idx)
                  → alloc_skb → dma_map_single(DMA_FROM_DEVICE)
                  → owner=DEV, state=POSTED
                  → rx_posted++
                  ← 返回 poll 循环，处理下一个 done desc

recv_stage04_frame.c: main()
  ├─ socket(AF_PACKET, SOCK_RAW, htons(0x88B7))
  │       → 只接收 ethertype 0x88B7 的帧
  ├─ bind(sockfd, &sockaddr_ll, ...)
  └─ recvfrom(sockfd, buf, BUFSIZE, 0, &src_addr, &addrlen)
          → 收到完整 Ethernet frame（含 header）
          → pr_info("len=%u protocol=0x88b7 from=%02x:...")
```

### 7.4 open / stop 路径

```
ifconfig nds4 up
  └─→ stage04_open(ndev)
          ├─ netif_start_queue(ndev)
          │       → 允许 ndo_start_xmit 被调用
          └─ napi_enable(&priv->napi)
                  → 允许 NAPI softirq schedule 本 poll 函数

ifconfig nds4 down
  └─→ stage04_stop(ndev)
          ├─ netif_stop_queue(ndev)
          │       → 禁止新的 ndo_start_xmit 调用
          └─ napi_disable(&priv->napi)
                  → 禁止 NAPI softirq schedule 本 poll 函数
```

### 7.5 ring replenishment 循环

```
                    ┌─────────────────────────────────────┐
                    │  stage04_init_rings()               │
                    │  for(i=0→ring_size)                 │
                    │    stage04_refill_rx_slot(priv, i)  │
                    │      → skb + dma_map + POSTED+DEV   │
                    └─────────────────────────────────────┘
                                        │
                                        ▼
                    ┌─────────────────────────────────────┐
                    │  TX: stage04_start_xmit()          │
                    │    stage04_find_posted_rx_slot()    │
                    │      → 找到一个 POSTED+DEV slot   │
                    │    memcpy(skb->data → rxd->skb)    │
                    │    rxd->state=DONE, rxd->owner=CPU │
                    │    rx_posted--, rx_done++           │
                    │    stage04_raise_irq()              │
                    │      → napi_schedule()              │
                    └─────────────────────────────────────┘
                                        │
                                        ▼
                    ┌─────────────────────────────────────┐
                    │  RX: stage04_poll()                 │
                    │    处理 DONE+CPU desc               │
                    │    rx_done--                        │
                    │    stage04_refill_rx_slot()        │
                    │      → 立刻补 fresh buffer          │
                    │      → rx_posted++                  │
                    └─────────────────────────────────────┘
                                        │
                    ┌───────────────────┴───────────────────┐
                    │  如果 ring 全满 → 回到 TX 路径       │
                    │  如果 ring 全空 → TX 路径丢包        │
                    └───────────────────────────────────────┘
```

### 7.6 DMA map/unmap 配对

| 路径 | map 方向 | unmap 方向 | 说明 |
|------|----------|------------|------|
| TX skb | `dma_map_single(..., DMA_TO_DEVICE)` | `dma_unmap_single(..., DMA_TO_DEVICE)` | CPU → device 发送 |
| RX buffer | `dma_map_single(..., DMA_FROM_DEVICE)` | `dma_unmap_single(..., DMA_FROM_DEVICE)` | device → CPU 接收 |
| RX refill | 同上 | 同上 | replenishment 复用同一对 API |

---

## 八、调试方法论

### 8.1 RX ethertype 错误排查路径

```
POLL PROTO=86dd (IPv6) 或 0800 (IPv4)
       │
       ├── TX ETH=88B7 ? ──YES──→ 问题在 send 端（frame 格式错误）
       │                        检查 send_stage04_frame 的 socket 类型
       │                        和 ethertype 解析进制
       │
       └── TX ETH 也错误 ──YES──→ 问题在 TX 路径 DMA/copy 逻辑
```

### 8.2 调试三板斧

```bash
# 1. dmesg 看 TX/RX 打印
sudo dmesg | grep stage04

# 2. debugfs stats 看计数
cat /sys/kernel/debug/netdev_stage04/stats

# 3. debugfs rings 看 ring 状态
cat /sys/kernel/debug/netdev_stage04/rings
```

### 8.3 hex dump 精确定位

当 frame 格式复杂时，用 hex dump 定位：

```c
pr_info("D0_5=%02x:%02x:%02x:%02x:%02x:%02x D6_11=%02x:%02x:%02x:%02x:%02x:%02x D12_15=%02x:%02x:%02x:%02x\n",
    skb->data[0], skb->data[1], skb->data[2], skb->data[3], skb->data[4], skb->data[5],
    skb->data[6], skb->data[7], skb->data[8], skb->data[9], skb->data[10], skb->data[11],
    skb->data[12], skb->data[13], skb->data[14], skb->data[15]);
// D12_15=88:b7:78:79 → 88:B7=ethertype, 78:79='xy' (payload 开头)
```

---

## 九、smoke test

```bash
# 加载
sudo insmod output/netdev_stage04.ko ifname=nds4
sudo ip link set nds4 up
sudo dmesg -c >/dev/null

# 发送
cd tools
sudo ./send_stage04_frame nds4 xyz123 88B7 3

# 验证
sudo dmesg | grep stage04
```

**预期输出**：
```
[stage04] TX RXIDX=21 SKBLEN=20 CPYLEN=20 ETH=88b7
[stage04] POLL IDX=21 LEN=20 PROTO=88b7 RC=0
```

| 字段 | 含义 | 预期值 |
|------|------|--------|
| `ETH` | TX 路径 skb->data[14:15]（ethertype） | `88b7` |
| `PROTO` | RX 路径 eth_type_trans 解析结果 | `88b7` |
| `RC` | netif_receive_skb 返回值（0=成功） | `0` |

---

## 十、与 stage03 的关系

**stage03 核心问题**：为什么需要 NAPI？中断只做 schedule，poll 按 budget drain pending queue。

**stage04 在 stage03 基础上引入**：

```
pending queue  ──────────────> descriptor ring
  （链表）                        （数组 + 读写位置指针）

skb 不是凭空出现  ────────────> 预投递 RX buffer（posted）

poll drain 后直接复用  ─────> replenishment（重新补 fresh buffer）
```

stage04 的 poll 函数同时体现两个阶段的概念：

```c
while (work_done < budget) {           // ← stage03: NAPI budget 概念
    // 找 DONE + CPU owner 的 desc     // ← stage04: ring ownership
    // 处理完立刻 refill                // ← stage04: replenishment
}
```

---

## 十一、踩坑记录（调试过程中发现的问题）

### 问题 1: `strtoul` 进制解析

```c
// 错误：strtoul("88B7", NULL, 0) → 88 (decimal)
// 'B' 不是合法十进制字符，解析在 'B' 处停止

// 正确：显式指定 hex
ethertype = (unsigned int)strtoul(argv[3], NULL, 16);
```

### 问题 2: `AF_PACKET SOCK_RAW` 绑定特定 ethertype

```c
// 错误：socket(AF_PACKET, SOCK_RAW, htons(specific_ethertype))
// kernel 在 sendto 时自动 prepend Ethernet header
// 但代码已经传入了完整 frame，导致双层 header

// 正确：ETH_P_ALL + 手动构造完整 frame
socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
// frame[0-5]=dst MAC, [6-11]=src MAC, [12-13]=ethertype, [14...]=payload
```

### 问题 3: `rmmod` 卡死（used=-1）

模块卸载时 `rmmod` 始终卡住，`lsmod` 显示 `Used = -1`。

根因是 `stage04_exit()` 的 cleanup 顺序错误，导致 `unregister_netdev()` 等待引用时死锁。

正确顺序：`napi_disable` → `netif_tx_disable` → `unregister_netdev` → `dev_remove_pack` → cleanup。

---

## 十二、参数说明

| 模块参数 | 默认值 | 说明 |
|---------|--------|------|
| `ifname` | nds4 | net_device 名称 |
| `ring_size` | 64 | TX/RX ring 深度 |
| `napi_weight` | 16 | poll 每次最多处理的包数 |
| `rx_buf_size` | 2048 | 预分配 RX buffer 大小 |

调小 `ring_size` 或 `napi_weight` 可以更容易观察到 budget 耗尽或 ring 紧张的现象。

---

## 十三、扩展方向

1. **真实 DMA transfer**：用 `dmaengine` API 替代 `memcpy`，实现真正意义的 DMA
2. **多队列**：扩展为多对 TX/RX queue pair（现代网卡多队列概念）
3. **virtio 后端**：对接 virtio-net 协议，作为 virtio frontend
4. **XDP 支持**：在 RX 路径添加 XDP（Express Data Path），实现 kernel bypass
5. **性能分析**：用 `perf record -g` 和 `ftrace function_graph` 分析 DMA 操作开销
