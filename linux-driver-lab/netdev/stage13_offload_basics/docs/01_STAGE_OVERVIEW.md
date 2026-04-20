# 01_STAGE_OVERVIEW — stage13 学习目标与 offload 概述

## stage13 学习目标

在 stage12 ethtool 控制面基础上，引入 **offload 基础能力**：

1. **feature 声明** — `NETIF_F_HW_CSUM`, `NETIF_F_SG`, `NETIF_F_GRO`, `NETIF_F_GSO_SOFTWARE`
2. **ip_summed 状态** — RX checksum 标记（`CHECKSUM_UNNECESSARY`）
3. **GRO 路径** — `napi_gro_receive()` vs `netif_receive_skb()` 差异
4. **feature 协商** — `ndo_set_features` 回调
5. **ethtool -k/-K** — offload 开关实验验证

---

## 什么是 offload

offload 是 NIC 驱动向内核协议栈声明"我可以替你做某些工作"的能力：

| 能力 | NETIF_F | 作用 |
|------|---------|------|
| RX checksum | `NETIF_F_RXCSUM` | 驱动已校验，协议栈不重复算 |
| TX checksum | `NETIF_F_HW_CSUM` | 驱动/DMA 计算 checksum |
| Scatter-Gather | `NETIF_F_SG` | 分散/聚集 I/O，减少拷贝 |
| GSO | `NETIF_F_GSO_SOFTWARE` | 协议栈分段，驱动聚合 |
| GRO | `NETIF_F_GRO` | 驱动合并同类包再上送 |

---

## 与 stage12 对比

| 维度 | stage12 | stage13 |
|------|---------|---------|
| features | 无 | `NETIF_F_HW_CSUM\|SG\|GRO\|GSO` |
| RX path | `netif_receive_skb()` | `napi_gro_receive()` |
| ip_summed | `CHECKSUM_NONE` | `CHECKSUM_UNNECESSARY` |
| ndo_set_features | 无 | 有 |
| ethtool -k | 无 | 显示所有 offload 能力 |
| ethtool -K | 无 | 可开关 GRO 等 |

---

## 为什么需要 offload

1. **性能** — 硬件/DMA 做 checksum 比软件快
2. **CPU 节省** — GRO 合并减少协议栈处理次数
3. **真实驱动** — 不会 offload 的驱动不是真实 NIC

---

## 核心概念：ip_summed

```
CHECKSUM_NONE          — 协议栈自己计算（默认）
CHECKSUM_UNNECESSARY  — 驱动/硬件已校验，协议栈不重复算
CHECKSUM_PARTIAL      — 驱动只算了部分（需要完整计算）
CHECKSUM_COMPLETE     — 驱动已完整计算
```

---

## 核心概念：GRO vs netif_receive_skb

```
netif_receive_skb(skb)   — 逐包上送，每个包单独处理
napi_gro_receive()        — 合并同类包，批量上送（减少 CPU 负载）
```

**GRO 合并条件**：同 5-tuple（src_ip, dst_ip, src_port, dst_port, proto）

---

## 核心概念：hw_features vs features vs wanted_features

```
hw_features     — 驱动支持的 offload 能力（ethtool -k 可查询）
wanted_features — 驱动希望启用的能力
features        — 当前实际启用的能力（= wanted & hw）
```

---

## 验证方法

```bash
# 1. 检查 offload 能力
ethtool -k nds13s

# 2. 开关 GRO，验证路径差异
ethtool -K nds13s gro off
ethtool -K nds13s gro on

# 3. 检查 feature 协商
ethtool -S nds13s | grep feature_set_count

# 4. debugfs 观测
cat /sys/kernel/debug/netdev_stage13_soft/offload
```
