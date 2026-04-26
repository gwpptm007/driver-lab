# 03_TROUBLESHOOTING

## guest A ping 不通 guest B

检查：

```bash
bridge link
bridge fdb show
ip link show tap-vnet-a
ip link show tap-vnet-b
ip link show br-vnet0
```

guest 内检查：

```bash
ip addr
ip link
arp -n
```

## bridge FDB 只看到一个 MAC

说明只有一个 guest 产生过流量或另一个 tap 没接好。  
让两个 guest 都 ping 一下 host bridge IP 或彼此 ping。

## tcpdump 看不到包

确认抓包位置：

```bash
sudo tcpdump -i tap-vnet-a -n -e
sudo tcpdump -i br-vnet0 -n -e
sudo tcpdump -i tap-vnet-b -n -e
```

如果 ping 流量太少，可以增加：

```bash
ping -i 0.2 192.168.100.3
```

## guest 没有 eth0

确认 QEMU 参数有：

```text
-device virtio-net-pci,netdev=net0
```

guest 内：

```bash
dmesg | grep -i virtio
ip link
```
