# 02_OFFLOAD_MODEL — stage13 offload 架构与 GRO

## 核心架构

stage13 在 stage12 ethtool 控制面基础上新增 **offload 能力声明与 GRO 路径**：

```
用户空间                    驱动                      内核协议栈
   │                         │                           │
   │ ethtool -k nds13s      │                           │
   │ ─────────────────────> │ get_flags()               │
   │ <───────────────────── │ rx-checksum: on          │
   │                         │ tx-checksum: on          │
   │                         │ sg: on, gso: on, gro: on  │
   │                         │                           │
   │ ethtool -K nds13s gro off                           │
   │ ─────────────────────> │ ndo_set_features()        │
   │                         │ ────────────────────────> │
   │                         │  features updated        │
   │                         │                           │
   │                         │ RX: build_skb()          │
   │                         │  → ip_summed=UNNECESSARY │
   │                         │  → napi_gro_receive()    │
   │                         │      (或 netif_receive)  │
```

---

## NETIF_F feature 声明

stage13 驱动在 `ndo_open` 或 `stage13_init()` 中设置：

```c
/* hw_features: 可被 ethtool -k 查询的能力 */
ndev->hw_features = NETIF_F_RXCSUM | NETIF_F_HW_CSUM |
                    NETIF_F_SG | NETIF_F_GSO_SOFTWARE | NETIF_F_GRO;

/* features: 驱动实际启用的能力（默认全部开启） */
ndev->features |= ndev->hw_features;

/* wanted_features: 驱动希望启用但可能未生效的能力 */
ndev->wanted_features = ndev->features;
```

---

## GRO 路径切换

在 `stage13_consume_rx_one()` 中：

```c
/* 1. 设置 checksum 状态
 *   CHECKSUM_UNNECESSARY = 驱动保证正确，协议栈不重复算
 */
skb->ip_summed = CHECKSUM_UNNECESSARY;

/* 2. GRO 路径切换
 *   - GRO enabled: napi_gro_receive() 合并同类包再上送
 *   - GRO disabled: netif_receive_skb() 逐包上送
 */
if (ndev->features & NETIF_F_GRO)
    napi_gro_receive(&q->napi, skb);
else
    netif_receive_skb(skb);

atomic64_inc(&q->stats.rx_gro_packets);
```

---

## TX checksum 检测

在 `stage13_start_xmit()` 中：

```c
/* 检测是否有 TX checksum offload 请求
 * CHECKSUM_PARTIAL = 驱动负责计算 checksum
 * (软模型无法真正做 DMA checksum，仅统计演示)
 */
if (skb->ip_summed == CHECKSUM_PARTIAL &&
    (ndev->features & NETIF_F_HW_CSUM))
    atomic64_inc(&q->stats.tx_csum_partial_count);

/* GSO 检测 */
if (skb_is_gso(skb) && (ndev->features & NETIF_F_GSO_SOFTWARE))
    atomic64_inc(&q->stats.tx_gso_packets);
```

---

## ndo_set_features — feature 协商

```c
static int stage13_set_features(struct net_device *ndev, netdev_features_t features)
{
    struct stage13_priv *priv = netdev_priv(ndev);
    int i;

    priv->last_features = ndev->features;
    ndev->features = features;
    for (i = 0; i < priv->num_queues; ++i)
        atomic64_inc(&priv->queues[i].stats.feature_set_count);
    return 0;
}
```

**调用时机**：`ethtool -K` 执行时会触发 `ndo_set_features`。

---

## ip_summed 状态机

| 状态 | 含义 | 协议栈行为 |
|------|------|-----------|
| `CHECKSUM_NONE` | 不确定 checksum | 协议栈自己计算 |
| `CHECKSUM_UNNECESSARY` | 已知正确 | 不重复计算 |
| `CHECKSUM_PARTIAL` | 部分计算 | 需要完整计算 |
| `CHECKSUM_COMPLETE` | 完整计算 | 相信此值 |

---

## GRO 合并条件

GRO 只合并满足以下条件的包：
- 同一 5-tuple（src_ip, dst_ip, src_port, dst_port, proto）
- 相同的 IP protocol
- TCP/UDP 相同 source port 和 dest port

**合并效果**：
- 减少协议栈中断处理次数
- 减少 CPU 消耗
- 提高吞吐

---

## debugfs offload 状态文件

```bash
cat /sys/kernel/debug/netdev_stage13_soft/offload
# 输出示例：
# features=0x1c03a rx_csum=1 tx_csum=1 sg=1 gso_sw=1 gro=1
```

---

## ethtool -k 输出示例

```
$ ethtool -k nds13s
rx-checksumming: on
tx-checksumming: on
scatter-gather: on
tcp-segmentation-offload: off
udp-fragmentation-offload: off
generic-segmentation-offload: on
generic-receive-offload: on
```

---

## ethtool -K 开关实验

```bash
# 关闭 GRO
ethtool -K nds13s gro off

# 开启 GRO
ethtool -K nds13s gro on

# 查看效果
ethtool -S nds13s | grep -E "rx_gro_packets|feature_set_count"
```
