# 00：15 分钟建立虚拟网络心智模型

## 先记住：一条路径、两种方向、六个参与者

在本 track 的最小拓扑中，guest 的 `eth0` 是 `virtio_net` 网卡；host 有 TAP 设备和 Linux bridge；QEMU 负责设备模型和控制面，`vhost_net` 可选择接手部分后端数据面。

```text
guest application
  -> guest TCP/IP + virtio_net
  -> virtqueue (shared-memory descriptor rings)
  -> QEMU userspace backend 或 host vhost_net
  -> host TAP net_device
  -> Linux bridge
  -> host local IP stack / another TAP / physical uplink
```

反方向按相反的所有权转移：host bridge 选择 TAP 出口，TAP 后端把帧放入 guest RX 可用 buffer，guest `virtio_net` 回收 used entry 后把帧交给 NAPI 和协议栈。

这不是六次普通函数调用，而是三类边界的组合：

| 边界 | 典型对象 | 要问的问题 |
| --- | --- | --- |
| guest 内核边界 | `net_device`、NAPI、virtqueue | 这个 buffer 还归 guest driver 吗？ |
| guest/host 设备边界 | vring、descriptor、eventfd | 谁可以读写 ring 的哪一部分？何时通知？ |
| host 网络边界 | TAP、bridge port、FDB | 帧进入的是 host L2 forwarding 还是 host IP stack？ |

## 不要混淆的五个对象

| 名称 | 它是什么 | 它不是什么 |
| --- | --- | --- |
| `virtio-net-pci` | guest 看到的 PCI virtio 网络设备 | host 的 TAP 名称 |
| virtqueue | guest driver 与后端协商的共享队列抽象 | Linux bridge 队列 |
| TAP | host 的 Ethernet net_device，和用户态/内核后端通过 FD 收发帧 | guest 内的网卡 |
| Linux bridge | host kernel 的 L2 forwarding/FDB 实现 | 硬件交换芯片，也不是 host 路由器 |
| `vhost_net` | host kernel 中可接管 tap 后端数据面的实现 | vhost-user，也不是 DPDK PMD |

## 以 ping 为例建立证据链

假设 guest `192.168.100.2` ping host bridge `192.168.100.1`：

1. guest 先 ARP；这会产生 broadcast，因此 bridge 在本实验中会 flood 到合格端口和 local delivery 路径。
2. guest 学到 bridge MAC 后再发 ICMP echo request；目的 MAC 是 host bridge 的 MAC，不是 TAP 的 MAC。
3. host 收到该帧后，bridge 既会学习 guest 源 MAC，也会把目的为本地 bridge MAC 的帧交给 host 协议栈。
4. reply 反向返回；bridge FDB 已有 guest MAC 时，会把帧定向转到对应 TAP 端口。

因此，`ping` 成功至少涉及 L2 邻居解析、L2 forwarding/local delivery、guest/host IP 配置和回程路径。它不是“virtio 已经完全正确”的唯一证据。

## 三个最常见的错误结论

### “TAP 收到帧，所以 guest 一定收到帧”

不成立。TAP 的观察点只证明帧达到 host TAP 一侧。后端是否把可用 RX buffer 填好、guest 是否回收 used entry、guest 是否在接口上接受该帧，需要别的证据。

### “guest A 能 ping guest B，所以 host IP 栈参与转发”

通常不成立。同一 bridge 内、FDB 已命中时，guest A 到 guest B 是 host kernel 的二层 bridge forwarding；帧不需要被 host 的 IP 路由栈转发。host CPU 仍会执行 bridge 逻辑。

### “启用 vhost 后速度一定更快”

不成立。`vhost=on` 只说明后端可以使用 `vhost_net`；性能还受 guest/host CPU、队列数、offload、工作负载、计时方式和虚拟化环境限制。先证明路径，再讨论吞吐或 RTT。

## 最小实验序列

1. 只创建 `br-vnet0`、`tap-vnet0`，用 `ip link` 和 `bridge link` 证明 host 拓扑。
2. 以 `-netdev tap,...` 加 `-device virtio-net-pci,...` 启动 guest，配置 IP 后 ping host bridge。
3. 保存 QEMU 参数、`bridge fdb show`、host/guest IP 状态和抓包记录。
4. 在相同 TAP/bridge 拓扑下切换 `vhost=off/on`；只改变一个变量。
5. 扩为两个 TAP、两个 guest，验证 FDB 先学习、后定向转发。

## 学完本章的自测

- 能否从任意一帧的源/目的 MAC 判断它更可能走 flood、FDB forwarding 还是 host local delivery？
- 能否解释为什么 guest 中看到的 `eth0` 与 host 的 `tap-vnet0` 是不同对象？
- 能否指出 QEMU、vhost_net、TAP、bridge 分别在哪一层接手/交出 buffer？
- 能否列出“ping 成功”之外至少三条路径证据？

下一章把这些参与者放进 guest、hypervisor 和 host 的明确责任边界中。
