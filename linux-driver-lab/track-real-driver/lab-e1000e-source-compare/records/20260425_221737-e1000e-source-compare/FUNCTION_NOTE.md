# FUNCTION NOTE

## 关键函数

| 函数 | 行号 | 职责 |
|------|------|------|
| `e1000_probe` | 7383 | PCI 设备初始化，分配 netdev + adapter，注册 netdev |
| `e1000_remove` | 7737 | PCI 设备移除，反注册 netdev，释放资源 |
| `e1000e_poll` | 2668 | NAPI poll 回调，处理 TX/RX cleanup |
| `e1000_clean_rx_irq` | 911 | RX IRQ 清理，分配 skb，提交 protocol stack |
| `e1000_clean_tx_irq` | 1212 | TX IRQ 清理，回收 buffer，唤醒 netif |

---

## e1000_probe (line 7383)

### 这段负责什么
PCI probe 函数，设备初始化入口：
1. `pci_enable_device()` → 启用 PCI 设备
2. `alloc_etherdev()` → 分配 net_device
3. `pci_set_drvdata()` → 关联 adapter
4. `e1000_sw_init()` → 软件初始化（分配 queues/irqs）
5. `register_netdev()` → 注册到 kernel

### 上游/下游
- 上游: PCI subsystem 调用 `e1000_probe(pdev, ent)`
- 下游: `e1000e_up()` (open) / `e1000e_down()` (close)

### 和 virtio_net 的不同点
- virtio: `virtnet_probe()` 通过 virtio_driver，设备是 virtio 总线上的
- e1000e: `e1000_probe()` 通过 pci_driver，设备是 PCI 总线上的

### 和自己哪个 stage 最像
- `stage01 netdev_skeleton` — alloc_etherdev + register_netdev 相同模式

---

## e1000_remove (line 7737)

### 这段负责什么
PCI remove 函数，设备移除时清理资源：
1. `unregister_netdev()` → 反注册 netdev
2. `pci_set_drvdata(pdev, NULL)` → 清除 driver data
3. 释放 TX/RX resources
4. 释放 IRQ

### 上游/下游
- 上游: PCI subsystem 调用 `e1000_remove(pdev)`
- 下游: `e1000e_down()` 实际清理

### 和 virtio_net 的不同点
- 模式相同，都是 probe/remove 对称结构
- virtio 的 remove 在 `virtnet_remove()` 中清理 virtqueue

### 和自己哪个 stage 最像
- `stage01` — probe/remove 对称性

---

## e1000e_poll (line 2668)

### 这段负责什么
NAPI poll 回调，batch 处理 TX/RX：
```c
e1000e_poll(napi, budget):
  tx_cleaned = e1000_clean_tx_irq(tx_ring)
  adapter->clean_rx(rx_ring, &work_done, budget)
  if (!tx_cleaned || work_done == budget)
    return budget  // 继续 poll
  napi_complete_done()  // 退出 poll
  e1000_set_itr()  // 更新 interrupt throttle rate
```

### 上游/下游
- 上游: NAPI subsystem 调用 `e1000e_poll()`
- 下游: `e1000_clean_tx_irq()` / `e1000_clean_rx_irq()`

### 和 virtio_net 的不同点
- virtio_net: 每个 rx_queue 有独立的 napi_struct，每个 RX queue 单独 poll
- e1000e: **整个 adapter 一个 napi**，TX/RX 在同一个 poll 函数里处理

### 和自己哪个 stage 最像
- `stage03 napi_poll` — `stage13_napi_poll()` 结构几乎相同
- 差异: e1000e 只有一个 napi，stage13 每个 rx_queue 一个 napi

---

## e1000_clean_rx_irq (line 911)

### 这段负责什么
RX 中断清理，处理 received packets：
1. `rx_desc = *rx_ring->desc` — 读 descriptor
2. 检查 EOP (End of Packet) 标志
3. `e1000_alloc_rx_buffers()` — 补充 RX buffer
4. `netif_receive_skb(skb)` — 提交到 protocol stack

变体: `e1000_clean_rx_irq_ps()` (page split 模式), `e1000_clean_jumbo_rx_irq()`

### 上游/下游
- 上游: `e1000e_poll()` 调用
- 下游: `netif_receive_skb()` → protocol stack

### 和 virtio_net 的不同点
- virtio_net: `virtnet_receive()` → `build_skb()` → `napi_gro_receive()` / `netif_receive_skb()`
- e1000e: 直接 `netif_receive_skb()`，无 GRO 路径（除非芯片支持）

### 和自己哪个 stage 最像
- `stage02 skb_path` — RX 路径的 skb 构建逻辑

---

## e1000_clean_tx_irq (line 1212)

### 这段负责什么
TX 中断清理，回收 TX descriptor：
1. 检查已完成的 TX descriptor
2. `e1000_put_txbuf()` — 释放 skb 和 DMA buffer
3. `netif_wake_queue()` — 唤醒被 stop 的 TX queue

### 上游/下游
- 上游: `e1000e_poll()` 调用
- 下游: `netif_wake_queue()` 唤醒 TX

### 和 virtio_net 的不同点
- virtio_net: `virtqueue_kick()` 后等待 host 消费，不需要主动清理
- e1000e: **必须主动遍历 TX ring** 找到已完成的 descriptor 并回收

### 和自己哪个 stage 最像
- `stage04 ring_dma` — ring descriptor 的分配和回收模式相同