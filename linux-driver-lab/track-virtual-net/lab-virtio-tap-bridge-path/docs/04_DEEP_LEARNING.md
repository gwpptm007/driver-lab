# 05_DEEP_LEARNING

## 完整拓扑

```
┌──────────────────────────────────────────────────────────────┐
│                         host                                 │
│                                                              │
│   ens33 (192.168.65.135)                                     │
│                                                              │
│   ┌─ br-vnet0 (192.168.100.1/24) ──────────────────────────┐ │
│   │                       ↑                                 │ │
│   │              ┌──── tap-vnet0 ──────────┐                │ │
│   │              │   (6e:71:9a:f3:fc:f8)   │                │ │
│   │              └──────────┬──────────────┘                │ │
│   └─────────────────────────┼────────────────────────────────┘
│                             │  Layer 2 (Ethernet frame)
└─────────────────────────────┼─────────────────────────────────┘
                              │
                    QEMU -netdev tap,ifname=tap-vnet0
                              │
┌─────────────────────────────┼─────────────────────────────────┐
│                         guest                                 │
│                                                              │
│   eth0 (virtio-net-pci)                                      │
│   MAC: 52:54:00:12:34:56                                     │
│   IP:  192.168.100.2/24                                       │
│                                                              │
│   内核驱动: virtio_net (virtio_net_probe → virtnet_open)     │
└──────────────────────────────────────────────────────────────┘
```

## QEMU 网络参数

```text
-netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no
-device virtio-net-pci,netdev=net0
```

- tap 后端: 用户态打开 `/dev/net/tun`，QEMU 直接读写 tap 文件描述符
- script=no/downscript=no: QEMU 不执行任何 ifup/ifdown 脚本，手工管理
- virtio-net-pci: PCI 设备，virtio 总线连接 virtio_net 驱动

## 数据包流转调用链（guest ping host bridge IP 192.168.100.1）

```
guest 用户空间 (ping 192.168.100.1)
  ↓
guest TCP/IP 协议栈 (构建 ICMP echo request, dst IP=192.168.100.1)
  ↓
guest ARP: 需要 192.168.100.1 的 MAC → 发送 ARP request，广播
  ↓
guest virtio_net 驱动
  -> ndo_start_xmit / start_xmit
  -> send queue
  -> virtqueue avail ring (descriptor put)
  -> kick backend (VirtqueueKick)
  ↓
QEMU virtio-net-pci 模拟器 (读 avail ring)
  ↓
host tap-vnet0 (字符设备 /dev/net/tun)
  - tap-vnet0 收到帧 (MAC 52:54:00:12:34:56, 协议 0x0800 = IPv4)
  ↓
Linux bridge (br-vnet0)
  - bridge 接收来自 tap-vnet0 的帧
  - 查 FDB 学习源 MAC: 52:54:00:12:34:56 → tap-vnet0 (新建条目)
  - 目的 MAC = br-vnet0 自己的 MAC (1e:f6:63:5c:99:02)
  - **目的 MAC 匹配 bridge 自身 → 帧不上转发，不走 FDB 查目的，直接上交 bridge 的 net_device**
  ↓
bridge net_device 接收处理 (br-vnet0 的 rx_handler)
  - 匹配本地 MAC，进入 IP 协议栈
  ↓
host IP 协议栈
  - ARP reply: 192.168.100.1 的 MAC 是 1e:f6:63:5c:99:02
  - ICMP echo request 匹配 192.168.100.1，接收并生成 echo reply
  ↓
回程: echo reply → br-vnet0 构造帧 → 查 FDB: 52:54:00:12:34:56 → tap-vnet0
     → tap-vnet0 写入 → QEMU tap fd → guest virtio_net → 用户空间
```

**关键点**: 本场景是 guest 和 host bridge IP 之间的 L3 连通性，不是"tap→bridge→tap"的纯 L2 转发。

- 当目的 MAC 是 bridge 自身的 MAC 时，bridge 不走转发逻辑，而是将帧交给本地 net_device 进入 IP 协议栈
- FDB 的实际作用是学习源 MAC，回程时查 FDB 找到目标 MAC 对应的端口
- 真正的"bridge L2 转发到另一个 port"的典型场景是 guest-to-guest 或 guest-to-other-host，此时目的 MAC 不是 bridge 自身 MAC，bridge 会查 FDB 决定从哪个端口转发

## FDB 学习过程（bridge fdb show 输出解读）

```
52:54:00:12:34:56 dev tap-vnet0 master br-vnet0
```

这是 QEMU virtio-net-pci 的标准 MAC（52:54:00:xx:xx:xx）。这条 FDB 条目是 bridge 自动学习的：

1. guest 发出 ICMP echo request 或 ARP（src MAC 52:54:00:12:34:56）
2. tap-vnet0 收到帧，交 bridge 处理
3. bridge 查 FDB：未见该 MAC → 学习
4. 在 tap-vnet0 上创建 FDB 条目 (state: learned)
5. 回程帧查 FDB: 52:54:00:12:34:56 → tap-vnet0，直接从该端口发出

FDB 学习省去了每次都洪泛到所有端口的开销，但 bridge 本身是 host kernel 软件模块，帧转发仍然经过 host CPU。

## 内核模块状态

```
bridge                425984  0
stp                    12288  1 bridge
llc                    16384  2 bridge,stp
```

- bridge.ko: 网桥实现（转发、洪泛、FDB 学习）
- stp.ko: 生成树协议（当前无 STP 拓扑，bridge 直接转发）
- llc.ko: IEEE 802.2 LLC（bridge 依赖）

tun.ko 在记录中未显示（但 plan_bridge_tap.sh 使用 `modprobe tun`，QEMU 运行时已加载）。

## 与 virtio_net 驱动视角的关联

| 层级 | virtio_net driver 视角 | 本 Lab 视角 |
|------|------------------------|-------------|
| 发送 | virtnet_send_command → avail ring → kick | QEMU 读取 descriptor → tap fd write |
| 接收 | NAPI poll → detach_buf → skb | tap fd read → 构造 skb → 上层 |
| 队列 | 每个 virtqueue 对应一个 NAPI 实例 | tap 是一个 TUN/TAP 字符设备 |
| 中断 | virtio 配置空间变化 → MSI-X | tap fd 可读 → QEMU 通知 guest |
| 缓冲 | page_pool 缓存区 | host tun/tap 缓冲区 |

**核心映射**: virtqueue 的 avail/used ring 是 guest 和 QEMU 之间的共享内存协议；tap 是 QEMU 和 host 之间的字符设备通道。两者组合完成 guest → host 的完整通路。

## 执行摘要

```
ping 测试: 5 packets transmitted, 5 received, 0% packet loss, time 4000ms
RTT: min/avg/max = 0.529/0.606/0.717 ms
```

ping 测试: 5 packets transmitted, 5 received, 0% packet loss, time 4000ms
RTT: min/avg/max = 0.529/0.606/0.717 ms

本 Lab 验证的是 guest ↔ host bridge IP 的 L3 连通性（ICMP ping），不是"tap→bridge→tap"的纯 L2 转发：
- guest 发出的帧目的 MAC 是 bridge 自身 MAC (1e:f6:63:5c:99:02)，bridge 交本地 net_device 进入 IP 协议栈
- 回程通过 FDB 学习到的 guest MAC 找到 tap-vnet0 端口发出
- 后续 lab-two-guest-bridge-flow 才是典型的纯 L2 转发场景