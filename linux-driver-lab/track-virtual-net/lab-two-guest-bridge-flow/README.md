# lab-two-guest-bridge-flow

> 所属：`track-virtual-net/`

## 一句话定位

这是 `track-virtual-net` 的第三个 Lab：

> **在已经跑通 single guest -> tap -> bridge -> host，以及 vhost=off/on 对照之后，启动两个 QEMU guest，让 guest A 与 guest B 通过同一个 Linux bridge 通信，并观察 tapA / bridge / tapB 上的二层转发路径。**

## 为什么做这个

前两个 Lab 已经分别解决：

1. `lab-virtio-tap-bridge-path`
   - guest virtio_net -> tap -> bridge -> host

2. `lab-virtio-vhost-kick-notify`
   - userspace backend vs vhost backend
   - kick / notify / eventfd / vhost_net

现在要补的是：

```text
guest A virtio_net
  -> tap A
  -> Linux bridge
  -> tap B
  -> guest B virtio_net
```

这条路径能帮助你把“单 guest 到 host”推进到“guest-to-guest 二层转发”。

## 本 Lab 的核心目标

1. 创建 `br-vnet0`
2. 创建 `tap-vnet-a` 和 `tap-vnet-b`
3. 启动两个 QEMU guest
4. guest A / guest B 均使用 virtio-net
5. guest A 能 ping guest B
6. host 在 tapA / br / tapB 上能抓到 ICMP
7. bridge FDB 能看到两个 guest MAC 学习结果

## 当前不做什么

- 不做复杂 NAT
- 不接外网
- 不做 DPDK / vhost-user
- 不做大规模性能结论
- 不做三台以上 guest 拓扑

## 推荐阅读顺序

1. `START_HERE.md`
2. `docs/01_GOAL_AND_SCOPE.md`
3. `docs/02_TOPOLOGY.md`
4. `docs/03_EXECUTION_FLOW.md`
5. `docs/04_ACCEPTANCE.md`
6. `docs/05_L2_FORWARDING_MODEL.md`
7. `docs/06_TROUBLESHOOTING.md`
8. `reports/two_guest_flow_report.md`
