# Virtual Network Path

## 路径定位

Virtual network path 用来理解 Linux host/guest 网络路径，重点是 tap、bridge、virtio、vhost、kick/notify 的协同。

这条路径要回答：

```text
虚拟机中的一个网络包如何通过 virtio frontend、vhost backend、tap、bridge 进入宿主机网络？
kick/notify 机制在前后端协作中解决什么问题？
```

## 覆盖范围

对应主线：

```text
linux-driver-lab/track-virtual-net/
```

主要实验：

| Lab/Project | 目标 |
|-------------|------|
| `lab-virtio-tap-bridge-path` | 理解 virtio/tap/bridge 基本路径 |
| `lab-virtio-vhost-kick-notify` | 对比 vhost on/off，理解 kick/notify 和后端处理 |
| `lab-two-guest-bridge-flow` | 两 guest 通过 bridge 的 L2 转发路径 |
| `project-virtual-net-end-to-end` | 收束 host/guest 虚拟化网络路径项目 |

## 关键机制

这条路径关注：

- QEMU virtio-net frontend。
- tap 设备作为 host 侧注入/收发接口。
- Linux bridge 的 L2 转发和 FDB。
- vhost-net 后端如何把数据面从 QEMU 进程搬到内核路径。
- kick/notify 如何在 guest/host virtqueue 之间推进事件。
- host/guest IP、bridge、tap、modules、QEMU args 如何共同构成可复现实验。

## 阶段价值

Virtual network path 把 netdev 能力放到云和虚拟化场景中：

```text
guest netdev
  -> virtio frontend
  -> virtqueue
  -> vhost/tap backend
  -> host bridge
  -> host/peer guest
```

这也是理解 DPDK vhost-user、virtio-user 之前的重要桥梁。

## Evidence 入口

主要证据索引：

- `../../track-virtual-net/README.md`
- `../../track-virtual-net/lab-virtio-tap-bridge-path/reports/lab-virtio-tap-bridge-path_report.md`
- `../../track-virtual-net/lab-virtio-vhost-kick-notify/reports/lab-virtio-vhost-kick-notify_report.md`
- `../../track-virtual-net/lab-two-guest-bridge-flow/reports/lab-two-guest-bridge-flow_report.md`
- `../../track-virtual-net/project-virtual-net-end-to-end/reports/review_bundle.md`
- [../evidence/virtual_net_evidence.md](../evidence/virtual_net_evidence.md)

## 当前边界

准确表述：

- 已完成 tap/bridge/vhost/kick/notify 的实验化理解和路径收束。
- 已有 host/guest 状态采集、QEMU args、bridge/tap records。

不要夸大：

- 不是完整云网络产品实现。
- 没有覆盖复杂 overlay、OVS/OVN、SR-IOV、多租户安全策略。
