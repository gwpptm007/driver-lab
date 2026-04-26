# SUMMARY

## 本轮目标
验证双 guest 通过 Linux bridge 做 L2 转发：guest A ping guest B 走 bridge FDB 转发路径。

## 拓扑
- host: br-vnet0 = 192.168.100.1/24
- tap-vnet-a: attached to br-vnet0, guest A 的 tap
- tap-vnet-b: attached to br-vnet0, guest B 的 tap
- guest A: eth0 = 192.168.100.2/24 (MAC 52:54:00:12:34:a1)
- guest B: eth0 = 192.168.100.3/24 (MAC 52:54:00:12:34:b1)

## QEMU 参数
guest A: `-netdev tap,id=net0,ifname=tap-vnet-a,script=no,downscript=no -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:a1`
guest B: `-netdev tap,id=net0,ifname=tap-vnet-b,script=no,downscript=no -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:b1`

## 测试结果

**FDB 学习**：
```
52:54:00:12:34:a1 dev tap-vnet-a master br-vnet0
52:54:00:12:34:b1 dev tap-vnet-b master br-vnet0
```
两个 guest MAC 均被 bridge 学习到。

**bridge link 状态**：
```
tap-vnet-a: state forwarding
tap-vnet-b: state forwarding
```
两个 tap 均 UP 且处于 forwarding 状态。

**ping 结果**：guest A ping guest B 成功，seq=4 RTT=2040ms（guest A 先启动8秒，ping等待时间较长）

## 当前结论
- guest A 和 guest B 均通过 virtio-net-pci 接入同一 bridge
- bridge FDB 正确学习两个 guest MAC 并关联到对应 tap 端口
- L2 转发路径确认：guest A → tap-vnet-a → br-vnet0 → tap-vnet-b → guest B
- 不经过 host IP stack，是纯 L2 bridge forwarding

## 问题与下一步
- RTT 较大（2040ms），因两个 guest 启动时间差导致
- 下一步: project-virtual-net-end-to-end 整合 single guest、vhost、two guest 三个场景