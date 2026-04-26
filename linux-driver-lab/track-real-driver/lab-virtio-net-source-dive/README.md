# lab-virtio-net-source-dive

> 把 stage00~stage14 的知识映射回真实 virtio_net 驱动

## 快速定位

| 你想看 | 去哪里 |
|--------|--------|
| 这个 Lab 是什么 | `docs/01_OVERVIEW.md` |
| TX 路径 | `docs/02_TX_RX_PATHS.md` |
| RX 路径 | `docs/02_TX_RX_PATHS.md` |
| queue / NAPI / feature | `docs/03_QUEUE_NAPI_FEATURES.md` |
| stage 映射 | `docs/04_STAGE_MAPPING.md` |
| 实际例子（Round1-3） | `records/examples/` |

## 核心结构

```
virtio_net.ko 关键结构
├── virtnet_info        ← 驱动私有数据（类似 stage12 priv）
├── virtnet_sq          ← 发送队列（struct virtqueue）
├── virtnet_rq          ← 接收队列
├── virtnet_affinity    ← 多队列 affinity
├── napi_struct         ← NAPI 轮询
└── netdev_ops          ← .ndo_start_xmit 等

数据路径
├── TX: start_xmit → xmit_set → virtqueue_add → virtnet_xmit_task
└── RX: virtnet_poll → receive_buf → skb → netif_receive_skb

Stage 映射
├── stage05-07 (char)  → probe/remove 骨架
├── stage08-11 (platform) → queue/NAPI 注册
├── stage12 (ethtool)    → .ndo_get_sset_count / .ndo_get_ethtool_stats
├── stage13 (offload)   → features/NETIF_F_*/gro_receive
└── stage14 (XDP)       → ndo_bpf / xdp_buff
```

## 三轮推进

### Round1: 架构 + probe
```
docs/01_OVERVIEW.md
docs/02_TX_RX_PATHS.md (architecture section)
→ 理解结构体、probe 骨架、queue 挂载
```

### Round2: TX + RX 主路径
```
docs/02_TX_RX_PATHS.md
→ 走通 start_xmit → virtqueue_add → doorbell
→ 走通 receive_buf → skb_build → netif_receive_skb
```

### Round3: feature + XDP
```
docs/03_QUEUE_NAPI_FEATURES.md
docs/04_STAGE_MAPPING.md
→ 把 stage13 offload / stage14 XDP 映射回真实代码
```

## 下一步

- `lab-virtio-net-runtime-observe` — 运行期观测验证
- `lab-virtio-net-ethtool-stats-mini-patch` — 小 patch 实验

## 关键文件

| 文件 | 用途 |
|------|------|
| `docs/02_TX_RX_PATHS.md` | TX/RX 代码路径 + 函数注释 |
| `docs/04_STAGE_MAPPING.md` | stage → virtio_net 映射表 |
| `records/examples/2026-demo-round1-arch/` | Round1 示范记录 |
| `records/examples/2026-demo-round2-txrx/` | Round2 示范记录 |