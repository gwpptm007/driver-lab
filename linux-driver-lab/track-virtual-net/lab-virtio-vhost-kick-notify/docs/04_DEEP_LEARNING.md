# 05_DEEP_LEARNING

## 完整拓扑（复用 lab-virtio-tap-bridge-path 基础）

```
guest (192.168.100.2) eth0 (virtio-net-pci)
       ↓ ndo_start_xmit / virtqueue kick
QEMU -netdev tap,vhost=<on|off>
       ↓
host tap-vnet0 → br-vnet0 (192.168.100.1) → host IP stack
```

## vhost=off 路径（QEMU userspace backend）

```
guest virtio_net 驱动
  -> ndo_start_xmit / start_xmit
  -> send queue
  -> virtqueue avail ring (descriptor put)
  -> kick backend (VirtqueueKick → eventfd write)
  ↓
QEMU userspace 主循环（tap fd 处理）
  - QEMU 监听 eventfd，收到 kick 后：
  - 读取 avail ring descriptor
  - 构造 Ethernet frame
  - write(tap_fd, frame)
  ↓
tap-vnet0 → br-vnet0 → host IP stack
```

**实测结果**: ping 5/5, RTT min/avg/max = 0.573/1.983/7.071 ms

## vhost=on 路径（vhost_net kernel backend）

```
guest virtio_net 驱动
  -> ndo_start_xmit / start_xmit
  -> send queue
  -> virtqueue avail ring (descriptor put)
  -> kick backend (VirtqueueKick → eventfd write)
  ↓
vhost_net kernel 模块（/dev/vhost-net）
  - QEMU 已提前通过 ioctl 将 virtqueue mmap 区域映射给 vhost_net
  - vhost_net 监听 eventfd，收到 kick 后：
  - 直接读取共享内存的 avail ring（无需 QEMU 介入）
  - 构造 frame，写入 tap fd（内核态，少了 userspace syscall）
  ↓
tap-vnet0 → br-vnet0 → host IP stack
```

**实测结果**: ping 5/5, RTT 0.777/2.350/8.223 ms
**无 Permission denied 警告**，vhost_net kernel backend 实际生效（重启后 wq7 已加入 kvm 组，vhost_net 模块已加载）

## kick/notify 方向模型

```
guest 侧 (virtio_net 驱动)
  |
  |  kick: descriptor 放入 avail ring, 写 eventfd
  |  方向: guest → backend (通知"有新工作")
  ↓
backend (QEMU userspace 或 vhost_net)
  |
  |  完成处理后写 callfd/eventfd 通知 guest
  |  方向: backend → guest (通知"已完成，可回收 buffer")
  ↓
guest 侧（used ring 更新，NAPI poll 回收 buffer）
```

**eventfd/kick/call 总结**：
- `kick`：guest → backend，通知 descriptor 可用
- `call`/`notify`：backend → guest，通知 buffer 已消费
- `eventfd`：提供 kick/call 的异步通知机制（Linux 匿名 fd）

## QEMU 参数对照

| 模式 | QEMU netdev 参数 | backend 位置 |
|------|-----------------|-------------|
| vhost=off | `-netdev tap,id=net0,...,vhost=off` | QEMU userspace |
| vhost=on | `-netdev tap,id=net0,...,vhost=on` | host kernel vhost_net |

**vhost=on 时 QEMU 仍负责**：
- 设备模型（virtio-net-pci 模拟）
- 控制面（virtqueue 初始化、MSI-X 中断配置）
- vhost-net fd ioctl（把 virtqueue 映射表传给 /dev/vhost-net）
- 启动/停止 guest

## 证据采集（如何证明 vhost=on 生效）

```bash
# host 侧
lsmod | grep vhost
ls -l /dev/vhost-net
cat /sys/class/vhost/vhost-*/* 2>/dev/null || true
bridge fdb show
ip link show tap-vnet0

# 对比 off/on 的 QEMU 参数差异
# off: vhost=off（或无 vhost 参数）
# on:  vhost=on
```

## 与 lab-virtio-tap-bridge-path 的关系

| 层级 | lab-virtio-tap-bridge-path | lab-virtio-vhost-kick-notify |
|------|---------------------------|------------------------------|
| guest 侧 | virtio_net → ndo_start_xmit | 同上（不变）|
| 数据路径 | QEMU tap backend → tap → bridge | 同上（tap/bridge 不变）|
| backend 位置 | 固定 QEMU userspace | 对比 QEMU userspace vs vhost_net |
| 关键变量 | 无（基础路径）| `vhost=off` vs `vhost=on` |

两个 Lab 合起来：从"guest→tap→bridge→host 能通"，推进到"backend 在 userspace 还是 kernel，路径差异是什么"。

## 后续 lab-two-guest-bridge-flow 的衔接

本 Lab 完成后，单 guest 路径已清晰。下一步扩展到：
```
guest A (tap-vnet-a) → bridge → (tap-vnet-b) guest B
```
这才是典型的"bridge L2 转发到另一个 port"场景，对应前面 problem 2 指出的方向。