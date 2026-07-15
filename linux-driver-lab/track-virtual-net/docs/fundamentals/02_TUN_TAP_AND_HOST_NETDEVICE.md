# 02：TUN/TAP 与 host net_device

## TAP 是什么

TAP 是 Linux TUN/TAP 驱动创建的 Ethernet 类型虚拟 net_device。它把 host 网络栈与持有 `/dev/net/tun` 文件描述符的后端连接起来：

- host kernel 向 TAP 发送帧时，后端从 FD **读取**这些 Ethernet 帧；
- 后端向 TAP FD **写入**Ethernet 帧时，host kernel 把它当作从该 TAP 网卡收到的入站帧。

TUN 与 TAP 的区别是承载的单位：TUN 处理三层 IP packet，TAP 处理二层 Ethernet frame。本 track 做 bridge/virtio-net，因此使用 TAP。

## 创建 TAP 不是“开一个名字”

`ip tuntap add dev tap-vnet0 mode tap user "$USER"` 会请求内核创建 net_device，并把可打开该设备的权限授予指定用户。后端还必须实际打开 `/dev/net/tun` 并通过 `TUNSETIFF` 绑定同名接口；QEMU 的 tap backend 正是在做这件事。

```bash
sudo modprobe tun
sudo ip tuntap add dev tap-vnet0 mode tap user "$USER"
sudo ip link set tap-vnet0 master br-vnet0
sudo ip link set tap-vnet0 up

ip -d link show tap-vnet0
bridge link show dev tap-vnet0
```

`ip link` 证明 net_device 存在，`bridge link` 证明它是 bridge port；二者都不证明 QEMU 已持有该设备 FD。

## TAP 的双向语义

| 方向 | 谁先产生帧 | 谁对 TAP 做操作 | host 视角 |
| --- | --- | --- | --- |
| guest TX -> host | guest virtio driver | backend 向 TAP FD 写 | host 从 `tap-vnet0` 收到入站帧 |
| host/bridge -> guest RX | bridge 选择 TAP 为 egress | backend 从 TAP FD 读 | host 向 `tap-vnet0` 发送出站帧 |

这张表解释一个常见误区：`tcpdump -i tap-vnet0` 看见帧时，要结合方向、MAC 和抓包时间判断它是在进入 host 还是离开 host；不要把“tap 上看到”直接等同为“guest 收到了”。

## `IFF_NO_PI`、持久化与 owner

常见 TAP 后端使用 `IFF_TAP | IFF_NO_PI`：

- `IFF_TAP` 表示 Ethernet frame；
- `IFF_NO_PI` 表示读写内容不带额外 packet information header，后端直接处理 Ethernet frame；
- persistent TAP 与一个临时打开的 FD 不是同一概念。实验脚本通常先创建 TAP，再由 QEMU 打开；清理时应删除自己创建的设备。

不要假设任意 QEMU 退出都会自动删除预先创建的 TAP。TAP 生命周期、bridge port 关系和 IP 配置应作为独立资源清理。

## 多队列不是把同一个 FD 复制多次

Linux 支持 multiqueue TUN/TAP，一个设备可由多个 FD/queue 并行服务。它需要后端、QEMU 设备配置、guest multiqueue 协商与 CPU/queue 映射共同支持。只把 `queues=N` 写入命令并不能证明已经实现并行数据面。

启用前至少验证：

1. host TAP 是否以 multiqueue 方式创建；
2. QEMU `-netdev` 与 `virtio-net-pci` 的 queue 配置是否匹配；
3. guest 是否协商多队列并创建相应 TX/RX queue；
4. 统计、IRQ、softirq 与吞吐是否显示预期分布。

## 安全边界

能够读写 TAP FD 的进程可以向 host 网络栈注入任意二层帧，也可能读到从该接口发出的帧。创建 TAP、设置 owner、是否让它加入 bridge、是否允许 promisc，都是安全决策，不只是实验便利设置。

最小实验中，TAP 应放在专用 bridge 和专用地址段；不要把不受信任 guest 的 TAP 直接接入生产 LAN。

## 可验证证据

| 问题 | 最小证据 | 仍不能证明什么 |
| --- | --- | --- |
| TAP 是否存在且 UP | `ip -d link show tap-vnet0` | QEMU 已绑定 FD |
| TAP 是否属于 bridge | `bridge link show dev tap-vnet0` | FDB 已学习 guest MAC |
| guest 是否发出帧到 host | TAP 抓包 + guest ping/ARP | virtqueue 完整时序 |
| host 是否向 guest 送帧 | TAP/bridge 抓包 + guest 接收计数 | guest 应用已处理 payload |

对应实践：[Lab 1 TAP/bridge 拓扑](../../lab-virtio-tap-bridge-path/docs/02_TOPOLOGY_AND_EXECUTION.md)。
