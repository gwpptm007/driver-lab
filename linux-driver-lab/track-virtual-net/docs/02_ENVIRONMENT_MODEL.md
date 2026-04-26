# 02_ENVIRONMENT_MODEL

## 推荐环境

宿主机：
- Linux host 或 Linux VM
- 可运行 QEMU
- 有 root/sudo 权限创建 tap / bridge
- 内核支持 tun/tap、bridge、vhost_net

guest：
- Linux 5.15.x 或你已有实验内核
- virtio_net 驱动可用
- rootfs 里至少有：`ip`、`ping`、可选：`iperf3`、`ethtool`

## 不推荐一开始就做的环境

- 直接在 VMware guest 的 e1000/vmxnet3 上验证 virtio_net
- 直接上 DPDK/vhost-user
- 直接做 NAT/iptables 复杂拓扑

## 最小拓扑

```
QEMU guest eth0(virtio_net)
        |
        | virtqueue
        |
QEMU -netdev tap,ifname=tap-vnet0
        |
host tap-vnet0
        |
host bridge br-vnet0
        |
host IP: 192.168.100.1/24
guest IP: 192.168.100.2/24
```