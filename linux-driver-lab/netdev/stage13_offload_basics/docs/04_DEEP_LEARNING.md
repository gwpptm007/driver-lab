# 04_DEEP_LEARNING — offload 与 GRO 核心调用链

## 1. ethtool -k / -K 系统调用链

### 1.1 用户空间 → 内核路径

```
用户: ethtool -k nds13s
    ↓
sys_ioctl()
    ↓
sock_ioctl()
    ↓
dev_ioctl()
    ↓
ethtool_ioctl()
    ↓
ethtool_get_flags()
    ↓
ndev->ethtool_ops->get_flags()  → stage13_get_flags()
```

```
用户: ethtool -K nds13s gro off
    ↓
ethtool_ioctl()
    ↓
ethtool_set_flags()
    ↓
__ethtool_set_flags()
    ↓
ndev->netdev_ops->ndo_set_features()  → stage13_set_features()
```

---

## 2. feature 声明完整调用链

```
stage13_soft_init()
    ↓
alloc_etherdev_mqs()           // 分配 netdev
    ↓
ndev->netdev_ops = &stage13_netdev_ops
ndev->ethtool_ops = &stage13_ethtool_ops
    ↓
ndev->hw_features = NETIF_F_RXCSUM | NETIF_F_HW_CSUM |
                    NETIF_F_SG | NETIF_F_GSO_SOFTWARE | NETIF_F_GRO;
    ↓
ndev->features |= ndev->hw_features  // 默认全部开启
    ↓
ndev->wanted_features = ndev->features
    ↓
register_netdev(ndev)
```

---

## 3. RX GRO 路径完整调用链

```
ethtool -K nds13s gro on  →  ndo_set_features() → ndev->features |= NETIF_F_GRO
    ↓
smoke test: 流量进入
    ↓
stage13_backend_workfn()        // 模拟 backend 填充 RX buffer
    ↓
stage13_refill_rx_slot()        // 从 page_pool 分配 page
    ↓
stage13_raise_irq()             // 触发中断模拟
    ↓
stage13_irq_workfn()
    ↓
napi_schedule_prep() + __napi_schedule()
    ↓
stage13_napi_poll()
    ↓
stage13_consume_rx_one()
    ↓
/* 1. build_skb 零拷贝 */
skb = build_skb(buf, priv->rx_buf_size);
    ↓
/* 2. 设置 ip_summed */
skb->ip_summed = CHECKSUM_UNNECESSARY;
    ↓
/* 3. GRO 路径判断 */
if (ndev->features & NETIF_F_GRO)
    napi_gro_receive(&q->napi, skb);  // ← GRO 路径
else
    netif_receive_skb(skb);           // ← 普通路径
    ↓
atomic64_inc(&q->stats.rx_gro_packets);
```

---

## 4. napi_gro_receive() 内部行为

```
napi_gro_receive(&q->napi, skb)
    ↓
netif_receive_skb_internal(skb, napi)
    ↓
gro_receive_list()
    ↓
/* GRO 合并逻辑：
 * 1. 遍历 gro_list 查找可合并的 skb
 * 2. 同 5-tuple 的包被合并到第一个匹配的 skb
 * 3. merged skb 被送入协议栈
 */
gro_normal_list() / gro_flush_final()
    ↓
netif_receive_skb(skb)         // 合并后的包上送协议栈
```

---

## 5. ndo_set_features 完整调用链

```
ethtool -K nds13s gro off
    ↓
ethtool_ioctl()
    ↓
ethtool_set_flags()
    ↓
__ethtool_set_flags()
    ↓
stage13_set_features(ndev, features)
{
    priv->last_features = ndev->features;
    ndev->features = features;  // 应用新 features
    for (i = 0; i < priv->num_queues; ++i)
        atomic64_inc(&priv->queues[i].stats.feature_set_count);
}
    ↓
用户通过 ethtool -S 看到 feature_set_count++
```

---

## 6. TX checksum 检测调用链

```
start_xmit()
    ↓
/* 检查是否有 TX checksum offload 请求 */
if (skb->ip_summed == CHECKSUM_PARTIAL &&
    (ndev->features & NETIF_F_HW_CSUM))
    atomic64_inc(&q->stats.tx_csum_partial_count);
    ↓
/* GSO 检测 */
if (skb_is_gso(skb) && (ndev->features & NETIF_F_GSO_SOFTWARE))
    atomic64_inc(&q->stats.tx_gso_packets);
    ↓
/* 继续 backend_copy */
```

---

## 7. hw_features vs features vs wanted_features 区别

```
hw_features     = 驱动支持的硬件能力（固定）
wanted_features = 驱动希望启用的能力（可变）
features        = 当前实际生效的能力（= wanted & hw）

示例：
- 硬件不支持 GSO → hw_features 不包含 NETIF_F_GSO
- 用户关闭 checksum → wanted_features 清除 NETIF_F_HW_CSUM
- features = wanted & hw → 实际生效的只是两者的交集
```

---

## 8. offload 与协议栈的边界关系

```
驱动视角：
    build_skb() → skb->ip_summed=UNNECESSARY → napi_gro_receive()
                                                       ↓
协议栈视角：                                    GRO 合并（或不合并）
    收到 skb → 检查 ip_summed → 如果 UNNECESSARY 则跳过 checksum 计算
```

---

## 9. 常见错误

### 9.1 GRO 时机判断错误

```c
/* 错误：在 build_skb 之前判断 GRO */
if (ndev->features & NETIF_F_GRO)   // ← 太早判断
    skb = build_skb(buf, ...);
else
    skb = build_skb(buf, ...);

/* 正确：在上送时判断 */
skb = build_skb(buf, ...);
skb->ip_summed = CHECKSUM_UNNECESSARY;
if (ndev->features & NETIF_F_GRO)
    napi_gro_receive(&q->napi, skb);
else
    netif_receive_skb(skb);
```

### 9.2 ip_summed 设置时机错误

```c
/* 错误：上送后才设置 */
netif_receive_skb(skb);
skb->ip_summed = CHECKSUM_UNNECESSARY;  // ← 太晚了

/* 正确：上送前设置 */
skb->ip_summed = CHECKSUM_UNNECESSARY;
napi_gro_receive(&q->napi, skb);
```

### 9.3 feature 协商未实现

```c
/* 错误：ndo_set_features 为空 */
static const struct net_device_ops stage13_netdev_ops = {
    .ndo_open = stage13_open,
    .ndo_stop = stage13_stop,
    .ndo_start_xmit = stage13_start_xmit,
    .ndo_set_features = NULL,  // ← 未实现
};

/* 正确：实现 ndo_set_features */
static const struct net_device_ops stage13_netdev_ops = {
    .ndo_open = stage13_open,
    .ndo_stop = stage13_stop,
    .ndo_start_xmit = stage13_start_xmit,
    .ndo_set_features = stage13_set_features,  // ← 正确
};
```

---

## 10. 参考资料

- `include/linux/netdev_features.h` — NETIF_F_* 定义
- `include/linux/skbuff.h` — ip_summed, GRO 相关
- `net/core/dev.c` — netif_receive_skb vs napi_gro_receive
- `Documentation/networking/checksum-offload.rst`
- `Documentation/networking/generic_segmentation_offload.rst`
- `Documentation/networking/gro.txt`
- virtio-net / igb 驱动源码
