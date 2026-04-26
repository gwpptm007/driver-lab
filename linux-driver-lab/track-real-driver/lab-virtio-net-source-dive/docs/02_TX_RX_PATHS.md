# 02_TX_RX_PATHS

## TX 路径 (发送)

```
ndo_start_xmit()
  ├─ skb 长度检查、分片检测
  ├─ virtnet_xmit_set()
  │   ├─ netdevsent = 更新统计
  │   ├─ xmit_more 检查（延迟批量提交）
  │   └─ virtqueue_add_outbuf() ← 添加 descriptor
  ├─ virtqueue_kick() ← 通知 host
  └─ TX completions 在 napi_poll() 中回收
```

关键函数：
- `start_xmit()` = `ndo_start_xmit` 入口
- `virtnet_xmit_set()` — 批量发送准备
- `virtqueue_add_outbuf()` — 放 buffer 到 virtqueue
- `virtqueue_kick()` — kick/gate 通知

对比 stage：
- stage02_skb_path — skb 生命周期
- stage04_ring_dma — ring 操作
- stage13_offload_basics — checksum offload

---

## RX 路径 (接收)

```
virtnet_poll() / napi_poll()
  ├─ budget 循环
  ├─ virtqueue_get_buf() ← 从 virtqueue 取 buffer
  ├─ skb = build_skb(page)
  ├─ GRO 判断（gro_receive vs netif_receive_skb）
  ├─ checksum 判断（CHECKSUM_NONE vs CHECKSUM_UNNECESSARY）
  └─ netif_receive_skb(skb) ← 上送协议栈

 refill 路径：
 virtnet_fill_rx() — 补 buffer 到 receive virtqueue
 (通常在 napi_complete 或 replenish 定时触发)
```

关键函数：
- `virtnet_poll()` = NAPI poll 入口
- `receive_buf()` — 收到数据后的处理
- `build_skb()` — page → skb 转换
- `gro_receive()` / `netif_receive_skb()` — 上送选择

对比 stage：
- stage03_napi_poll — poll 模式
- stage11_page_pool_rx — page recycle
- stage13_offload_basics — GRO/GSO
- stage14_xdp_basics — XDP 入口点

---

## 关键差异（真实驱动 vs 教学驱动）

| 方面 | 教学驱动 | virtio_net |
|------|----------|------------|
| TX kick | 直接 kick | 可延迟批量（xmit_more） |
| RX buffer | 固定 page | 复杂 recycle/refill 逻辑 |
| NAPI | 单一 poll | 多队列各自的 napi_struct |
| offload | 简单开关 | feature negotiation + 多层判断 |
| XDP | 基本入口 | 与 GRO/skbuff 路径交叉 |

---

## 验证方法

运行 `lab-virtio-net-runtime-observe` 时：
- `netif_receive_skb` trace = RX 主路径
- `net_dev_queue` trace = TX 发送队列入口
- RX 增量和 trace events 对照