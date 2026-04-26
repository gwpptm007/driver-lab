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

## vhost=on 路径（预期，未能实测）
```
guest virtio_net (ndo_start_xmit → virtqueue avail ring → kick)
  → eventfd → /dev/vhost-net → vhost_net kernel 模块
  → write(tap_fd) (内核态，少了 userspace syscall)
  → tap-vnet0 → br-vnet0 → host IP stack
```
- backend: host kernel vhost_net
- 本应: QEMU 通过 ioctl 将 virtqueue 映射传给 vhost_net，vhost_net 直接读共享内存

## 实际测试结果
- 第一轮（vhost=on Permission denied fallback）：
  - QEMU 输出: `warning: tap: open vhost char device failed: Permission denied`
  - wq7 用户不在 kvm 组，/dev/vhost-net 不可用
  - QEMU 自动 fallback 回 userspace backend
  - ping 成功（走了 vhost=off 路径）
- 第二轮（vhost=on Permission denied fallback）：
  - 同上，rootfs 缺少 ping 工具，无 ping 输出
  - kvm 组权限已通过 `sudo usermod -aG kvm wq7` 修复（wq7 重登录后生效）
  - 真实 vhost=on 路径需要 wq7 重新登录以刷新组Membership

## 关键差异
| 维度 | vhost=off | vhost=on |
|------|-----------|---------|
| backend | QEMU userspace | vhost_net kernel（权限不足时 fallback） |
| 数据路径 | QEMU 主循环 + syscall | vhost_net 直接读 virtqueue 共享内存 |
| /dev/vhost-net | 不需要 | 需要（当前权限不足） |

## 证据
- QEMU args off: `vhost=off`
- QEMU args on: `vhost=on`
- modules: bridge, stp, llc（vhost_net 未加载）
- dev nodes: /dev/vhost-net owned by kvm group
- ping: off 5/5, on 5/5（fallback 路径）