# 02_AF_XDP_RING_MODEL

AF_XDP 的关键不是普通 socket API，而是 UMEM 和四类 ring。

## UMEM

UMEM 是用户态提前分配的一块 packet buffer 区域。内核和用户态通过 descriptor 传递这块内存里的 frame 地址。

## FILL ring

用户态把可用 frame 地址放进 FILL ring，告诉内核：这些 buffer 可以用来收包。

## RX ring

内核收到包后，把 packet descriptor 放进 RX ring。用户态从 RX ring 取 descriptor，再根据地址访问 UMEM 中的数据。

## TX ring

用户态如果要发包，把待发送 packet descriptor 放进 TX ring。当前 lab 不做 TX 发送，只创建 TX ring，为后续 forwarder 做准备。

## COMPLETION ring

TX 完成后，内核把 descriptor 放进 COMPLETION ring，用户态可以回收 frame。

## 当前 lab 的最小循环

```text
fill UMEM frame addresses
    ↓
XDP redirect packet to XSK
    ↓
read RX descriptor
    ↓
count packet/bytes
    ↓
return frame address to FILL ring
```
