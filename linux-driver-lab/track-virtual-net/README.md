# track-virtual-net

把虚拟化网络路径串起来：guest driver → hypervisor backend → host virtual device → host switching → host NIC / another guest

## 四个阶段

| 阶段 | 目录 | 目标 |
|------|------|------|
| 1 | `lab-virtio-tap-bridge-path/` | 建立 tap/bridge 拓扑，guest 能 ping host bridge IP |
| 2 | `lab-virtio-vhost-kick-notify/` | 对比 QEMU userspace backend 与 vhost=on |
| 3 | `lab-two-guest-bridge-flow/` | 双 guest L2 转发，guest A 能 ping guest B |
| 4 | `project-virtual-net-end-to-end/` | 收成作品，拓扑/路径/trace/ping 完整报告 |

详细目标、环境说明、推荐顺序、后续方向 → `docs/01_TRACK_GOAL.md`