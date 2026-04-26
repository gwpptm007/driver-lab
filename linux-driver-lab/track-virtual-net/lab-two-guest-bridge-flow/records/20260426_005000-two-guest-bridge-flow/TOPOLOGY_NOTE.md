# TOPOLOGY_NOTE

## host
- br-vnet0: 192.168.100.1/24, MAC 1e:f6:63:5c:99:02, state UP
- tap-vnet-a: MAC 5a:be:d3:19:5d:1f, master br-vnet0, state UP, forwarding
- tap-vnet-b: MAC 56:1c:35:8d:e9:d2, master br-vnet0, state UP, forwarding
- 内核模块: bridge, stp, llc

## guest A (QEMU A)
- eth0: virtio-net-pci, MAC 52:54:00:12:34:a1
- IP: 192.168.100.2/24
- 驱动: virtio_net

## guest B (QEMU B)
- eth0: virtio-net-pci, MAC 52:54:00:12:34:b1
- IP: 192.168.100.3/24
- 驱动: virtio_net

## IP plan
- host bridge: 192.168.100.1/24
- guest A: 192.168.100.2/24
- guest B: 192.168.100.3/24
- 子网: 192.168.100.0/24

## L2 path
```
guest A (192.168.100.2) eth0
  -> QEMU A tap-vnet-a
  -> br-vnet0 (FDB lookup: 52:54:00:12:34:b1 -> tap-vnet-b)
  -> tap-vnet-b
  -> QEMU B
  -> guest B eth0 (192.168.100.3)
```