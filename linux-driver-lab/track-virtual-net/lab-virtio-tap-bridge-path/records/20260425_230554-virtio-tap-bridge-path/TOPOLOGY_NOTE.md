# TOPOLOGY NOTE

## host
- br-vnet0: 192.168.100.1/24, MAC 1e:f6:63:5c:99:02, state UP, master bridge
- tap-vnet0: MAC 6e:71:9a:f3:fc:f8, master br-vnet0, state UP
- ens33: 192.168.65.135/24 (物理网络，不参与本拓扑)
- 内核模块: bridge, stp, llc 已加载

## guest
- eth0: virtio-net-pci, MAC 52:54:00:12:34:56 (QEMU 默认 virtio MAC)
- IP: 192.168.100.2/24
- 驱动: virtio_net (内核模块)

## tap
- tap-vnet0: TUN/TAP 字符设备，mode tap
- QEMU 通过 /dev/net/tun 读写 tap fd
- tap 收到帧直接交 bridge 处理

## bridge
- br-vnet0: 学习型 bridge
- FDB 条目: 52:54:00:12:34:56 → tap-vnet0 (自动学习)
- 无 STP (STP 关闭，point-to-point 链路)

## QEMU args
```
-netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no
-device virtio-net-pci,netdev=net0
```

## IP plan
- host bridge: 192.168.100.1/24
- guest eth0: 192.168.100.2/24
- 子网: 192.168.100.0/24