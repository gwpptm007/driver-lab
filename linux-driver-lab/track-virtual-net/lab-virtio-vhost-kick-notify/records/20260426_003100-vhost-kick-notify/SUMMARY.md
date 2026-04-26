# SUMMARY

## 本轮目标
在 tap/bridge 拓扑下，对比 vhost=off 和 vhost=on 两种后端模式，验证 guest 网络连通性并采集 host 状态证据。

## 拓扑
- host: br-vnet0 = 192.168.100.1/24, tap-vnet0 attached to br-vnet0
- guest: eth0 = 192.168.100.2/24 (virtio-net-pci)

## vhost=off 结果
```
PING 192.168.100.1: 5 packets transmitted, 5 received, 0% packet loss
round-trip min/avg/max = 0.392/1.848/6.531 ms
```
- QEMU userspace 处理 tap fd
- ping 成功，完整路径：guest virtio_net → QEMU tap fd → tap-vnet0 → br-vnet0 → host IP stack

## vhost=on 结果
```
PING 192.168.100.1: 5 packets transmitted, 5 received, 0% packet loss
round-trip min/avg/max = 0.777/2.350/8.223 ms
```
- vhost_net kernel backend 实际生效
- 无 Permission denied 警告（重启后 wq7 已加入 kvm 组，vhost_net 模块已加载）
- 数据面路径：guest virtio_net → vhost_net kernel → tap-vnet0 → br-vnet0

## QEMU 参数对照
| 模式 | 参数 |
|------|------|
| vhost=off | `-netdev tap,id=net0,...,vhost=off` |
| vhost=on | `-netdev tap,id=net0,...,vhost=on` |

## 当前结论
- 两种模式均 5/5 ping 成功
- vhost=on 时 vhost_net kernel backend 生效，数据面绕过 QEMU userspace 主循环
- host 模块确认：vhost_net, vhost, vhost_iotlb, tap 均已加载

## 问题与下一步
- 两轮 RTT 差异不明显（0.392 vs 0.777 ms），可能需要更大 traffic 才能观察到差异
- 下一步: lab-two-guest-bridge-flow 验证纯 L2 转发场景