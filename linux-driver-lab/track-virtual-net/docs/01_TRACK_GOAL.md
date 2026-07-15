# 01_TRACK_GOAL

## 目标

`track-virtual-net` 的目标不是再读一个单独驱动，而是把虚拟化网络路径串起来：

```text
guest driver
  -> hypervisor backend
  -> host virtual device
  -> host switching
  -> host NIC / another guest
```

## 和前面 Track 的关系

### 来自 `netdev`
你已经知道：
- netdev 生命周期、NAPI、queue、IRQ
- ethtool/stats、offload/XDP 基础

### 来自 `track-real-driver`
你已经知道：
- `virtio_net` 的 guest 前端视角
- `e1000/e1000e` 的传统 NIC 对照视角

### 本 Track 要补什么
补完整系统协同：
- QEMU 网络参数
- tap 设备、Linux bridge
- vhost_net
- guest-to-host / guest-to-guest 路径

## 推荐顺序

开始 Lab 前，先按 [fundamentals/README.md](fundamentals/README.md) 建立 TAP、bridge、virtio、virtqueue 与 vhost 的边界模型。基础文档不替代下面的执行步骤；它解释每一步为什么存在、应留下什么证据，以及哪些结论不能仅由 ping 推出。

1. `lab-virtio-tap-bridge-path/` — 基础路径：guest → tap → bridge → host
2. `lab-virtio-vhost-kick-notify/` — backend 从 userspace 扩到 vhost
3. `lab-two-guest-bridge-flow/` — 单 guest 扩到双 guest L2 转发
4. `project-virtual-net-end-to-end/` — 收成完整项目

不要一上来同时处理 guest、host、vhost、bridge、two guest、DPDK。

## 四个阶段目标

### Phase 1：`lab-virtio-tap-bridge-path/`
- 建立 tap / bridge 拓扑
- guest 能 ping host bridge IP
- host 能在 tap/bridge 上抓到包
- 画出完整路径图

验收：`ip link show`、`bridge link`、guest ping 结果、`tcpdump` 记录

### Phase 2：`lab-virtio-vhost-kick-notify/`
- 对比 QEMU userspace backend 与 `vhost=on`
- 理解 eventfd / kick / notify / call
- 观察 vhost_net 模块与 tap 关系

验收：QEMU 参数对照、`lsmod | grep vhost`、`vhost=off` vs `vhost=on` 对照说明

### Phase 3：`lab-two-guest-bridge-flow/`
- 启动两个 QEMU guest，均使用 virtio-net
- tapA/tapB 接入同一个 bridge
- guest A ping guest B
- bridge FDB 学习两个 guest MAC

验收：guest A/B IP 记录、bridge FDB、ping 结果、A → bridge → B 路径图

### Phase 4：`project-virtual-net-end-to-end/`
- 把前面三个 Lab 收成一个作品
- 对比 userspace backend 和 vhost backend
- 形成最终报告和分享稿
- 为后续 DPDK/vhost-user 铺路

验收：final report、topology diagram、qemu command set、records bundle

## 当前最小闭环

```
host 创建 bridge
host 创建 tap
QEMU guest 使用 virtio-net-pci 接入 tap
guest 配 IP
host bridge 配 IP
guest ping host bridge IP
host 抓 tap/bridge 包
输出 records 与路径图
```

第一轮只做：`QEMU guest + virtio-net + tap + bridge + ping`
