# SUMMARY

## 本轮目标
在 tap/bridge 拓扑下，对比 vhost=off 和 vhost=on 两种后端模式，验证 guest 网络连通性并采集 host 状态证据。

## 拓扑
- host: br-vnet0 = 192.168.100.1/24, tap-vnet0 attached to br-vnet0
- guest: eth0 = 192.168.100.2/24 (virtio-net-pci)
- QEMU args off: `-netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=off -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56`
- QEMU args on:  `-netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=on  -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56`

## vhost=off 结果
```
PING 192.168.100.1: 5 packets transmitted, 5 received, 0% packet loss
round-trip min/avg/max = 0.573/1.983/7.071 ms
```
- QEMU userspace 处理 tap fd
- ping 成功，网络路径完整

## vhost=on 结果
```
PING 192.168.100.1: 5 packets transmitted, 5 received, 0% packet loss
round-trip min/avg/max = 0.418/2.091/7.452 ms
```
- QEMU 启动日志: `warning: tap: open vhost char device failed: Permission denied`
- 因 wq7 用户无 /dev/vhost-net 读写权限（仅 kvm 组可访问），QEMU fallback 回 userspace backend
- 网络仍然通，实际走了 vhost=off 的 userspace 路径

## 当前结论
- 两种模式在当前环境均 5/5 ping 成功
- vhost=on 时 /dev/vhost-net 权限不足导致 fallback，这是用户组权限问题，不是路径问题
- 解决方式: `sudo usermod -aG kvm wq7` 然后重新登录，或改用 root 执行 QEMU

## 问题与下一步
- /dev/vhost-net 权限问题需要 sudo 解决
- 下一步: 以 root 身份重跑 vhost=on，对比 host kernel vhost_net 和 userspace backend 的实际差异
- 或继续 lab-two-guest-bridge-flow 验证纯 L2 转发场景