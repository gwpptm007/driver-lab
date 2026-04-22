# 01_STAGE_OVERVIEW — stage14 学习目标与 XDP 概述

## stage14 学习目标

在 stage13 offload 基础能力基础上，引入 **XDP（eXpress Data Path）**：

1. **XDP 位置** — 数据包最早处理点，比 GRO 更早
2. **xdp_buff** — XDP 的数据包结构，替代 skb
3. **xdp_action** — `XDP_PASS` / `XDP_DROP` / `XDP_TX` / `XDP_REDIRECT`
4. **ndo_bpf 回调** — 驱动注册 XDP handler
5. **XDP 统计** — `xdp_pass`, `xdp_drop`, `xdp_tx`, `xdp_redirect`

---

## 什么是 XDP

XDP（eXpress Data Path）是 Linux 内核网络栈中最早期的处理点，在网卡收到数据包后、内核协议栈之前处理：

```
Packet arrives at NIC
        ↓
[XDP] ←--- stage14（最早处理点，在 build_skb 之前）
        ↓
[GRO] ←--- stage13
        ↓
[netif_receive_skb] ←--- stage13
        ↓
[protocol stack]
```

---

## 为什么需要 XDP

1. **超低延迟** — 在内核协议栈之前处理，避免昂贵的 skb 分配
2. **高性能** — 可以达到接近线速的 DDoS 防护、负载均衡
3. **DPDK/AF_XDP 基础** — 理解 XDP 是理解用户空间网络绕过的前提

---

## 与 stage13 对比

| 维度 | stage13 | stage14 |
|------|---------|---------|
| 处理点 | GRO 之后 | XDP 之后（更早） |
| 数据结构 | sk_buff | xdp_buff（更轻量） |
| 处理函数 | napi_gro_receive | bpf_prog_run |
| ndo 回调 | ndo_set_features | ndo_bpf |
| offload 能力 | GRO/GSO/checksum | XDP program |
| ethtool -S | rx_gro_packets | xdp_pass/drop/tx/redirect |

---

## XDP vs GRO

| 维度 | XDP | GRO |
|------|-----|-----|
| 处理时机 | 数据包最早点 | netif_receive_skb 之前 |
| 数据结构 | xdp_buff（不分配 skb） | skb（需要分配） |
| 处理位置 | 驱动层 | 网络层 |
| 灵活性 | BPF program 可编程 | 固定合并逻辑 |
| 用途 | DDoS防护、负载均衡、转发 | 减少协议栈中断 |

---

## xdp_buff 结构

```c
struct xdp_buff {
    void *data;      // 数据包起始指针
    void *data_end;  // 数据包结束指针
    void *data_meta; // 元数据区域（可用于传递信息）
};
```

注意：`xdp_buff` 没有 `skb->protocol`、`skb->dev` 等字段，它是最轻量的数据包表示。

---

## xdp_action 类型

| Action | 含义 | 驱动行为 |
|--------|------|----------|
| `XDP_PASS` | 放行数据包 | 继续走 build_skb → GRO → netif_receive_skb |
| `XDP_DROP` | 丢弃数据包 | page 直接归还 page_pool，不上送协议栈 |
| `XDP_TX` | 发送数据包 | 将 page 直接发到另一个设备（需要 TX 路径） |
| `XDP_REDIRECT` | 重定向 | 通过 `xdp_do_redirect()` 发到其他设备或 BPF map |

---

## 验证方法

```bash
# 1. 检查 XDP 状态
ip link show nds14s

# 2. 加载 XDP program
ip link set dev nds14s xdp obj xdp_count.o sec test

# 3. 检查 XDP 统计
ethtool -S nds14s | grep xdp_
cat /sys/kernel/debug/netdev_stage14_soft/xdp

# 4. 卸载 XDP
ip link set dev nds14s xdp off
```
