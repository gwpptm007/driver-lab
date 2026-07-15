# track-virtual-net

把虚拟化网络路径串起来：guest driver → hypervisor backend → host virtual device → host switching → host NIC / another guest

## 第一次进入先读这里

先阅读 [docs/fundamentals/README.md](docs/fundamentals/README.md)。它从 virtio-net、virtqueue、TAP、Linux bridge、vhost_net 到双 guest FDB 转发建立统一模型，再进入各 Lab。基础文档只解释机制、验证边界和可扩展方向；具体命令、records 和验收仍以 Lab 为准。

```text
fundamentals
  -> lab-virtio-tap-bridge-path
  -> lab-virtio-vhost-kick-notify
  -> lab-two-guest-bridge-flow
  -> project-virtual-net-end-to-end
```

## 四个阶段

| 阶段 | 目录 | 目标 |
|------|------|------|
| 1 | `lab-virtio-tap-bridge-path/` | 建立 tap/bridge 拓扑，guest 能 ping host bridge IP |
| 2 | `lab-virtio-vhost-kick-notify/` | 对比 QEMU userspace backend 与 vhost=on |
| 3 | `lab-two-guest-bridge-flow/` | 双 guest L2 转发，guest A 能 ping guest B |
| 4 | `project-virtual-net-end-to-end/` | 收成作品，拓扑/路径/trace/ping 完整报告 |

详细目标、环境说明、推荐顺序、后续方向 → `docs/01_TRACK_GOAL.md`
