# 03_AF_XDP_MODEL

## 路径

```text
NIC/netdev
  -> XDP
  -> XSKMAP
  -> AF_XDP socket
  -> UMEM
  -> user-space rings
```

## 核心对象

- XDP program
- xskmap
- AF_XDP socket
- UMEM
- fill ring
- completion ring
- RX ring
- TX ring
- copy mode
- zero-copy mode
