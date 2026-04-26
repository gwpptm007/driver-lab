# 01_GOAL_AND_SCOPE

## 目标

在同一套 tap/bridge 拓扑下，对比：

```text
vhost=off:
  guest virtio_net
    -> virtqueue
    -> QEMU userspace backend
    -> tap
    -> bridge

vhost=on:
  guest virtio_net
    -> virtqueue
    -> eventfd/kick
    -> vhost_net kernel backend
    -> tap
    -> bridge
```

## 本 Lab 要回答的问题

1. QEMU userspace backend 和 vhost backend 的边界是什么？
2. `vhost_net` 介入后，QEMU 是否完全退出数据路径？
3. kick / notify / call / eventfd 分别表达什么方向的事件？
4. 如何从 host 状态和 QEMU 参数证明当前启用了 vhost？
5. 这个实验和前面的 `virtio_net` 驱动分析如何衔接？

## 当前不做什么

- 不做 vhost-user / DPDK
- 不做复杂吞吐性能结论
- 不做内核代码修改
- 不做 two guest

## 拓扑

```text
guest eth0(virtio_net): 192.168.100.2/24
host br-vnet0:          192.168.100.1/24
host tap-vnet0:         attached to br-vnet0
```

### vhost=off 参数

```text
-netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=off
-device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56
```

### vhost=on 参数

```text
-netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=on
-device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56
```

**注意**: `vhost=on` 不等于"完全不经过 CPU"。它表示 virtio-net 后端数据面更多由 host kernel 的 `vhost_net` 处理，host CPU 仍然参与，只是路径和上下文不同。

## 最低通过

- `vhost=off` guest 能 ping host bridge IP
- `vhost=on` guest 能 ping host bridge IP
- records 中有两轮 QEMU 参数记录
- records 中有两轮 host state 记录

## 标准通过

在最低通过基础上，再满足：

- 能确认 `/dev/vhost-net` 存在
- 能确认 `vhost_net` 模块状态
- 有 `vhost=off` 与 `vhost=on` 的对照说明
- 有 `KICK_NOTIFY_NOTE.md`

## 优秀通过

- 能画出 userspace backend vs vhost backend 两张路径图
- 能说明 QEMU 在 `vhost=on` 时仍负责控制面/设备模拟等职责
- 能解释 eventfd/kick/call 与 virtqueue 事件推进的关系

## 推荐记录

每轮都记录：

```bash
ip -br link
ip addr
bridge link
bridge fdb show
lsmod | grep -E 'vhost|tun|bridge'
ls -l /dev/vhost-net /dev/net/tun
ps -ef | grep qemu
```
