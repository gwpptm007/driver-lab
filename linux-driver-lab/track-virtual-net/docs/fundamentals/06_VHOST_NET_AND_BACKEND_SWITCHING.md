# 06：vhost_net 与后端切换

## 为什么需要 vhost

最初的 TAP backend 可以由 QEMU userspace 处理 guest virtqueue 与 TAP FD 的收发。它简单、容易理解，但高频数据路径会经过 QEMU userspace 的调度和复制/通知处理。Linux `vhost_net` 提供一个内核后端：QEMU 仍负责建立设备、协商和配置，但在符合条件时把 virtqueue 的数据面处理交给 host kernel 的 vhost worker。

```text
vhost=off
guest virtqueue <-> QEMU userspace tap backend <-> TAP <-> bridge

vhost=on
guest virtqueue <-> host vhost_net worker <-> TAP <-> bridge
                 QEMU 保留设备模型与控制面
```

这是一条后端实现替换，不是 guest 网卡或 host L2 拓扑替换。

## `vhost=on` 前置条件

至少需要：

1. host 内核有可用的 `vhost_net` 支持；
2. `/dev/vhost-net` 可访问；
3. QEMU tap backend 和 virtio-net 配置支持该模式；
4. guest/host 协商的 queue、memory mapping、eventfd 设置可被 vhost 接受；
5. 仍然存在可用的 TAP/bridge 基础路径。

```bash
sudo modprobe vhost_net
ls -l /dev/vhost-net
lsmod | grep -E 'vhost_net|vhost|tun|bridge'
```

模块和设备节点只是前置条件。是否实际绑定成功还要结合 QEMU stderr、进程参数、运行期状态和对照记录判断。

## 正确设计 vhost=off/on 对照实验

唯一应改变的是 backend 模式：

```text
same guest image
same guest MAC/IP
same tap/bridge
same QEMU machine and virtio-net options
same workload and duration
only: vhost=off vs vhost=on
```

若两轮同时改变 QEMU 版本、CPU pinning、guest offload、tap 名称或 IP，RTT/吞吐差异不再能归因给 vhost。

## vhost 仍不等于“零拷贝、无调度、硬件直通”

vhost_net 可以减少 userspace backend 参与，但数据路径仍可能受到：

- guest/host 调度与 CPU 争用；
- queue 数量和 notification 策略；
- TAP/bridge/host networking 的处理；
- offload、GRO/GSO、包大小和 workload；
- 虚拟化平台、内存映射、IOMMU 与版本实现差异。

因此本 Lab 的成功目标是解释路径差异和建立对照证据，而不是在没有严谨基线时宣称固定百分比性能提升。

## vhost_net、vhost-user、virtio-user 的边界

| 名称 | 后端位置 | 典型用途 | 本 track 是否验证 |
| --- | --- | --- | --- |
| `vhost_net` | host kernel | QEMU TAP backend 加速 | 是 |
| vhost-user | 外部 userspace process | DPDK/OVS userspace backend | 否，后续 DPDK track |
| virtio-user | userspace virtio frontend | 与 vhost-user 对接 | 否，后续 DPDK track |

它们共享部分 virtio/vhost 术语，但进程边界、socket/FD 传递、内存共享和错误模型不同。报告中必须写全名称。

## 观测与结论边界

建议每次对照保留：

- 完整 QEMU command line；
- `vhost=off/on` 参数与 QEMU stderr；
- `/dev/vhost-net`、模块和 TAP/bridge 状态；
- guest/host IP、MAC、FDB 与 ping/iperf workload；
- CPU、queue、offload、时间窗口等性能上下文；
- 失败时的原因和回退到 `vhost=off` 的结果。

“两种模式下 FDB 学习相同”是合理的 L2 结论；“vhost 一定提高性能”不是本实验自动能得出的结论。

对应实践：[vhost kick/notify Lab](../../lab-virtio-vhost-kick-notify/START_HERE.md)。
