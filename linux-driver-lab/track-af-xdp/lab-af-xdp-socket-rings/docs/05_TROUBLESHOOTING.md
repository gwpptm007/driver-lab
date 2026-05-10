# 05_TROUBLESHOOTING

## xsk_socket__create failed

常见原因：

- 网卡不是内核驱动，例如仍绑定在 `uio_pci_generic/vfio-pci`；
- `AF_XDP_IFACE` 写错；
- queue id 不存在；
- libbpf/xsk headers 版本不匹配。

建议：

```bash
ip link show ens192
ethtool -i ens192
ls /sys/class/net/ens192/queues
```

## native 模式失败

VMware/vmxnet3 环境建议先用：

```bash
AF_XDP_MODE=skb AF_XDP_BIND_MODE=copy
```

native/zero-copy 留到后续阶段比较。

## rx_packets 一直是 0

这通常不是 socket 初始化问题，而是没有流量进入目标 RX queue。

处理方式：

- 在另一台 VM/宿主机向 `ens192` 所在网络发 ping/UDP/ARP；
- 增大运行时长：`AF_XDP_DURATION=30`；
- 确认收包队列：当前默认 queue 0。
