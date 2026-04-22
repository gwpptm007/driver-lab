# 05_ACCEPTANCE_AND_NEXT_STEP

## 最低通过标准

1. 能说清 `virtio_net` 的总体结构
2. 能列出关键结构体与关键函数分组
3. 能画出 TX/RX 主路径
4. 能建立 `stage00~stage14 ↔ virtio_net` 的映射表

## 本 Lab 完成后的推荐下一步

### 路线 A：继续真实驱动线
- `lab-real-driver-ethtool-stats`
- `lab-real-driver-small-patch`

### 路线 B：切到虚拟化协同线
- `track-virtual-net/lab-virtio-vhost-kick-notify`
