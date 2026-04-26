# VHOST COMPARE NOTE

## vhost=off 路径
```
guest virtio_net (ndo_start_xmit → virtqueue avail ring → kick)
  → QEMU userspace 主循环 (tap fd 处理)
  → write(tap_fd, frame)
  → tap-vnet0 → br-vnet0 → host IP stack
```
- backend: QEMU userspace
- tap fd: QEMU 直接读写

## vhost=on 路径
```
guest virtio_net (ndo_start_xmit → virtqueue avail ring → kick)
  → eventfd → /dev/vhost-net → vhost_net kernel 模块
  → write(tap_fd) (内核态，少了 userspace syscall)
  → tap-vnet0 → br-vnet0 → host IP stack
```
- backend: host kernel vhost_net
- QEMU 通过 ioctl 将 virtqueue 映射表传给 vhost_net
- vhost_net 直接读共享内存 virtqueue，无 QEMU userspace 中转

## 实测结果
| 模式 | ping | RTT min/avg/max |
|------|------|-----------------|
| vhost=off | 5/5 | 0.392/1.848/6.531 ms |
| vhost=on | 5/5 | 0.777/2.350/8.223 ms |

- vhost=on 无 Permission denied 警告，vhost_net kernel backend 实际生效
- QEMU 参数差异: `vhost=off` vs `vhost=on`
- host modules: vhost_net, vhost, vhost_iotlb, tap 均已加载

## 关键差异
| 维度 | vhost=off | vhost=on |
|------|-----------|---------|
| backend | QEMU userspace | vhost_net kernel |
| 数据路径 | QEMU 主循环 + syscall | vhost_net 直接读 virtqueue 共享内存 |
| /dev/vhost-net | 不需要 | 需要（已加载） |

## 证据
- QEMU args off: `vhost=off`
- QEMU args on: `vhost=on`
- modules: vhost_net, vhost, vhost_iotlb, tap（on 时）vs userspace only（off 时）
- ping: off 5/5, on 5/5