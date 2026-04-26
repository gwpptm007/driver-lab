# TOPOLOGY NOTE

## host
- br-vnet0: 192.168.100.1/24, MAC 1e:f6:63:5c:99:02, state UP, master bridge
- tap-vnet0: MAC 6e:71:9a:f3:fc:f8, master br-vnet0, state UP
- ens33: 192.168.65.135/24 (物理网络，不参与本拓扑)
- 内核模块: bridge, stp, llc 已加载

## guest (virtio-net-pci)
- eth0: virtio-net-pci, MAC 52:54:00:12:34:56
- IP: 192.168.100.2/24
- 驱动: virtio_net (内核模块)

## QEMU args 对照
| 模式 | netdev 参数 |
|------|-------------|
| vhost=off | `-netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=off` |
| vhost=on |  `-netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=on` |

## IP plan
- host bridge: 192.168.100.1/24
- guest eth0: 192.168.100.2/24
- 子网: 192.168.100.0/24