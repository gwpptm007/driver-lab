# 05_DEEP_LEARNING

## 完整拓扑（双 guest L2 转发）

```
       guest A (192.168.100.2)                  guest B (192.168.100.3)
       eth0 MAC: 52:54:00:12:34:a1              eth0 MAC: 52:54:00:12:34:b1
              ↓                                        ↑
   QEMU A -netdev tap,ifname=tap-vnet-a    QEMU B -netdev tap,ifname=tap-vnet-b
              ↓                                        ↑
       tap-vnet-a ←———————— br-vnet0 ————————→ tap-vnet-b
                    (192.168.100.1/24)
                         host
```

## 数据包流转调用链（guest A ping guest B）

```
guest A 用户空间 (ping 192.168.100.3)
  ↓
guest A TCP/IP 协议栈 (构建 ICMP echo request, dst IP=192.168.100.3)
  ↓
guest A ARP: 需要 192.168.100.3 的 MAC → ARP request 广播（泛洪）
  ↓
guest A virtio_net 驱动 (ndo_start_xmit → virtqueue → kick)
  ↓
QEMU A virtio-net-pci 模拟器 (读取 avail ring)
  ↓
tap-vnet-a (字符设备 /dev/net/tun)
  ↓
Linux bridge (br-vnet0)
  - 接收来自 tap-vnet-a 的帧
  - 查 FDB 源 MAC: 52:54:00:12:34:a1 → tap-vnet-a (学习条目)
  - 查 FDB 目的 MAC: 52:54:00:12:34:b1 → tap-vnet-b (已学习)
  - **FDB 命中**：直接从 tap-vnet-b 端口转发，不上 IP 栈
  ↓
tap-vnet-b
  ↓
QEMU B virtio-net-pci 模拟器
  ↓
guest B virtio_net 驱动 (NAPI poll → 接收数据)
  ↓
guest B TCP/IP 协议栈 (匹配本地 IP 192.168.100.3，生成 ICMP echo reply)
  ↓
回程: guest B → tap-vnet-b → br-vnet0 → 查 FDB → tap-vnet-a → guest A
```

**关键点**: ping 192.168.100.3 时，目的 MAC 不是 bridge 自身 MAC，而是 guest B 的 MAC。bridge 查 FDB 直接从 tap-vnet-b 端口转发，全程走 bridge L2 forwarding，不经过 host IP stack。

## FDB 学习过程解读

实测 FDB：
```
52:54:00:12:34:a1 dev tap-vnet-a master br-vnet0
52:54:00:12:34:b1 dev tap-vnet-b master br-vnet0
```

- `52:54:00:12:34:a1` 是 guest A 的 MAC，bridge 知道它在 tap-vnet-a 后面
- `52:54:00:12:34:b1` 是 guest B 的 MAC，bridge 知道它在 tap-vnet-b 后面

FDB 学习顺序：
1. guest A 上线，发包 → bridge 学习 `a1 → tap-vnet-a`
2. guest B 上线，发包 → bridge 学习 `b1 → tap-vnet-b`
3. 后续双向通信，FDB 命中直接转发，无 flooding

## bridge link 状态解读

```
tap-vnet-a: state forwarding (不是 disabled/flooding)
tap-vnet-b: state forwarding
```

- `state forwarding`: 端口正常转发状态，FDB 已学习，不需要 flooding
- 如果 state 是 `disabled` 或 FDB 未学习，会 flooding 到所有端口

## guest-to-host vs guest-to-guest 路径对比

| 场景 | 路径 | 关键判断 |
|------|------|----------|
| guest → host bridge IP | guest → tap → bridge → host IP stack | 目的 MAC = bridge 自身 MAC，进入 IP 栈 |
| guest A → guest B | guest A → tapA → bridge → tapB → guest B | 目的 MAC ≠ bridge 自身 MAC，走 L2 转发 |

**两者的本质区别**：
- guest-to-host: 目的 MAC 是 bridge 自身，bridge 交本地 net_device 进 IP 协议栈
- guest-to-guest: 目的 MAC 是其他 guest，bridge 查 FDB 直接从对应 tap 端口转发

## 与前面 Lab 的关系

| Lab | 路径 | 理解重点 |
|-----|------|----------|
| lab-virtio-tap-bridge-path | guest → tap → bridge → host IP | 目的 MAC = bridge 自身，进入 IP 栈 |
| lab-virtio-vhost-kick-notify | vhost=off/on 对比 | backend 位置差异，不改变 L2 转发逻辑 |
| lab-two-guest-bridge-flow | guest A → tapA → bridge → tapB → guest B | 目的 MAC ≠ bridge 自身，纯 L2 转发 |

三个 Lab 串联：单一 guest 基础路径 → vhost backend 对比 → 双 guest L2 转发，完整覆盖了 virtio_net + tap + bridge 的核心场景。

## 执行摘要

```
guest A ping guest B: 成功
FDB: 52:54:00:12:34:a1 → tap-vnet-a, 52:54:00:12:34:b1 → tap-vnet-b
bridge link: tap-vnet-a/tap-vnet-b 均为 forwarding 状态
路径: guest A → tap-vnet-a → br-vnet0 → tap-vnet-b → guest B
特点: 纯 L2 转发，不经过 host IP stack
```