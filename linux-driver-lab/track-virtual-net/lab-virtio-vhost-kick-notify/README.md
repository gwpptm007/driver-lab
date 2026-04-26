# lab-virtio-vhost-kick-notify

> 所属：`track-virtual-net/`

## 一句话定位

这是 `lab-virtio-tap-bridge-path` 之后的第二个 Lab：

> **在已经跑通 guest virtio_net -> tap -> bridge -> host 的基础路径后，对比 QEMU userspace backend 与 Linux kernel `vhost_net` backend，理解 virtqueue kick/notify/eventfd/call 的运行边界。**

## 为什么现在进入这个 Lab

上一轮 `lab-virtio-tap-bridge-path` 已经证明：

```text
guest virtio_net
  -> QEMU tap backend
  -> tap-vnet0
  -> br-vnet0
  -> host IP stack
```

这条基础链路能跑通。

现在要继续回答：

1. QEMU userspace backend 和 `vhost_net` backend 到底差在哪？
2. `vhost=off` 和 `vhost=on` 的 QEMU 参数如何对照？
3. `vhost_net` 介入后，virtqueue 的后端消费路径如何变化？
4. kick / notify / eventfd / call 在 host/guest 协同里分别处于什么层次？
5. 哪些证据能证明当前用了 vhost，而不是普通 userspace tap backend？

## 本 Lab 不追求什么

当前不做：

- DPDK vhost-user
- 大规模性能结论
- 内核 patch
- 复杂 NAT / 多 bridge
- two guest 拓扑

当前只做：

- 同一套 tap/bridge 拓扑
- `vhost=off` 一轮
- `vhost=on` 一轮
- host state / qemu args / ping / 可选 iperf
- 形成对照记录

## 推荐阅读顺序

1. `START_HERE.md`
2. `docs/01_GOAL_AND_SCOPE.md`
3. `docs/02_TOPOLOGY.md`
4. `docs/03_EXECUTION_FLOW.md`
5. `docs/04_ACCEPTANCE.md`
6. `docs/05_KICK_NOTIFY_MODEL.md`
7. `docs/06_USERSPACE_VS_VHOST.md`
8. `docs/07_TROUBLESHOOTING.md`
9. `reports/vhost_compare_report.md`
